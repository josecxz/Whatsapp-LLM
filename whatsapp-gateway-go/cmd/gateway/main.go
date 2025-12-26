package main

import (
	"context"
	"fmt"
	"log"
	"os"
	"os/signal"
	"syscall"
	"time"

	_ "github.com/mattn/go-sqlite3"
	"github.com/mdp/qrterminal/v3"
	"go.mau.fi/whatsmeow"
	"go.mau.fi/whatsmeow/store/sqlstore"
	"go.mau.fi/whatsmeow/types/events"
	waLog "go.mau.fi/whatsmeow/util/log"

	"whatsapp-gateway/internal/config"
	"whatsapp-gateway/internal/serializer"
	"whatsapp-gateway/internal/transport"
)

func main() {
	// 1. Cargar Configuración
	// Esto inicializa las variables de entorno para BackendURL y StoreDSN
	config.Init()
	ctx := context.Background()
	// 2. Configurar Logging
	// Usamos el logger nativo de whatsmeow para consistencia
	clientLog := waLog.Stdout("Client", "INFO", true)
	dbLog := waLog.Stdout("Database", "WARN", true) // Menos ruido en DB
	handlerLog := waLog.Stdout("Handler", "INFO", true)

	// 3. Inicializar Base de Datos de Sesión (SQLite)
	// Esta DB solo guarda las claves de encriptación y la sesión, NO los mensajes.
	container, err := sqlstore.New(ctx, "sqlite3", config.StoreDSN, dbLog)
	if err != nil {
		log.Fatalf("❌ Error fatal inicializando SQLStore: %v", err)
	}

	// 4. Obtener el dispositivo (Sesión)
	deviceStore, err := container.GetFirstDevice(ctx)
	if err != nil {
		log.Fatalf("❌ Error obteniendo dispositivo: %v", err)
	}

	// 5. Crear el Cliente de WhatsApp
	client := whatsmeow.NewClient(deviceStore, clientLog)

	// 6. Registrar el Manejador de Eventos
	// Aquí es donde capturamos los mensajes entrantes
	client.AddEventHandler(func(evt interface{}) {
		switch v := evt.(type) {
		case *events.Message:
			handleMessage(ctx, client, v, handlerLog)
		case *events.HistorySync:
			// Opcional: Manejar carga de historial si lo deseas en el futuro
			handlerLog.Infof("📚 HistorySync recibido con %d conversaciones (ignorado por ahora)", len(v.Data.Conversations))
		}
	})

	// 7. Lógica de Conexión y Login
	if client.Store.ID == nil {
		// --- CASO 1: Primera vez (Login con QR) ---
		qrChan, _ := client.GetQRChannel(context.Background())
		err = client.Connect()
		if err != nil {
			log.Fatalf("❌ Fallo al conectar: %v", err)
		}

		for evt := range qrChan {
			if evt.Event == "code" {
				fmt.Println("\n⚠️  ESCANEA ESTE CÓDIGO QR CON WHATSAPP  ⚠️")
				qrterminal.GenerateHalfBlock(evt.Code, qrterminal.L, os.Stdout)
			} else if evt.Event == "success" {
				fmt.Println("✅ Login exitoso!")
			}
		}
	} else {
		// --- CASO 2: Reconexión (Sesión existente) ---
		err = client.Connect()
		if err != nil {
			log.Fatalf("❌ Fallo al reconectar: %v", err)
		}
		log.Println("✅ Conectado a WhatsApp (Sesión restaurada)")

		// TRUCO IMPORTANTE: Sincronización de Contactos
		// Esperamos un momento y forzamos la carga de la lista de contactos.
		// Sin esto, GetChatName devolverá números en lugar de nombres.
		go func() {
			time.Sleep(2 * time.Second)
			log.Println("🔄 Sincronizando agenda de contactos...")
			contacts, err := client.Store.Contacts.GetAllContacts(ctx)
			if err == nil {
				log.Printf("👥 Agenda cargada: %d contactos disponibles para resolución de nombres.", len(contacts))
			} else {
				log.Printf("⚠️ No se pudo leer la agenda de contactos: %v", err)
			}
		}()
	}

	// 8. Graceful Shutdown (Mantener vivo hasta Ctrl+C)
	c := make(chan os.Signal, 1)
	signal.Notify(c, os.Interrupt, syscall.SIGTERM)

	// Bloquea aquí hasta recibir la señal
	<-c

	log.Println("👋 Cerrando conexión y guardando estado...")
	client.Disconnect()
}

// handleMessage es el núcleo del Ingestor.
// 1. Filtra. 2. Resuelve Nombre. 3. Serializa. 4. Envía a C++.
func handleMessage(ctx context.Context, client *whatsmeow.Client, evt *events.Message, logger waLog.Logger) {
	// A. FILTROS
	// Ignorar actualizaciones de estado (historias)
	if evt.Info.Chat.String() == "status@broadcast" {
		return
	}
	// Ignorar mensajes vacíos (protocolo, reacciones extrañas)
	if evt.Message == nil {
		return
	}

	// B. RESOLUCIÓN DE NOMBRE
	// Usamos la función robusta que consulta la DB interna de whatsmeow
	realName := GetChatName(ctx, client, evt)

	// C. SERIALIZACIÓN
	// Convertimos el evento complejo de WhatsApp en un JSON plano y seguro
	payload := serializer.SerializeMessage(evt, realName)

	// D. ENVÍO AL BACKEND (HTTP)
	// Enviamos al puerto configurado (ej: 8080 en C++)
	err := transport.SendToBackend(payload, config.BackendURL)

	if err != nil {
		logger.Errorf("⚠️ Error enviando a C++: %v", err)
	} else {
		// Log limpio para ver qué pasa en tiempo real
		msgType := payload["message_type"]
		logger.Infof("🚀 Enviado a C++ [%s] De: %s (%s)", msgType, realName, evt.Info.Sender.User)
	}
}

// GetChatName determina el nombre legible del remitente.
// Prioridad: "Yo" > Nombre de Grupo > Nombre en Agenda > PushName > Número
func GetChatName(ctx context.Context, client *whatsmeow.Client, msg *events.Message) string {
	senderJID := msg.Info.Sender
	chatJID := msg.Info.Chat

	// 1. Si el mensaje lo envié yo mismo
	if msg.Info.IsFromMe {
		return "Yo (Sistema)"
	}

	// 2. Si es un GRUPO
	if chatJID.Server == "g.us" {
		// Intentamos obtener el nombre del grupo
		// NOTA: Según tu main (1).go, tu versión no usa context aquí.
		if info, err := client.GetGroupInfo(ctx, chatJID); err == nil {
			// Opción: Devolver "Grupo: Nombre (Usuario)"
			// return fmt.Sprintf("%s (%s)", info.Name, senderJID.User)
			return info.Name
		}
		return "Grupo " + chatJID.User
	}

	// 3. Si es un CONTACTO INDIVIDUAL (Buscar en Agenda)
	// Buscamos por el JID del sender.
	// NOTA: Según tu main (1).go, tu versión no usa context aquí.
	if info, err := client.Store.Contacts.GetContact(ctx, senderJID); err == nil && info.Found {
		if info.FullName != "" {
			return info.FullName // El nombre con el que lo guardaste
		}
		if info.PushName != "" {
			return info.PushName // El nombre que él se puso
		}
	}

	// 4. Si no está en agenda, usamos el PushName que viene en el mensaje
	if msg.Info.PushName != "" {
		return msg.Info.PushName
	}

	// 5. Fallback: Devolver solo el número
	return senderJID.User
}
