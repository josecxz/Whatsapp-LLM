package serializer

import (
	"go.mau.fi/whatsmeow/types/events"
)

// SerializeMessage convierte el evento de WhatsApp en un mapa JSON seguro.
// 'customName' es opcional: si envías una cadena vacía "", usará el PushName por defecto.
func SerializeMessage(evt *events.Message, customName string) map[string]interface{} {
	msg := evt.Message
	var content string

	// Usamos los métodos Get...() que son más seguros que verificar punteros nil manualmente
	switch {
	case len(msg.GetConversation()) > 0:
		content = msg.GetConversation()
	case len(msg.GetExtendedTextMessage().GetText()) > 0:
		content = msg.GetExtendedTextMessage().GetText()
	case len(msg.GetImageMessage().GetCaption()) > 0:
		content = msg.GetImageMessage().GetCaption()
	case len(msg.GetVideoMessage().GetCaption()) > 0:
		content = msg.GetVideoMessage().GetCaption()
	default:
		content = "" // Mensaje multimedia sin texto o tipo desconocido
	}

	// Lógica de nombre: Si le pasamos un nombre calculado (de la agenda), usamos ese.
	// Si no, usamos el PushName (nombre público) del evento.
	finalName := customName
	if finalName == "" {
		finalName = evt.Info.PushName
	}

	return map[string]interface{}{
		"id":         evt.Info.ID,
		"chat_jid":   evt.Info.Chat.User,   // ID del chat (Grupo o Usuario)
		"sender_jid": evt.Info.Sender.User, // ID de quien escribe
		"sender":     finalName,            // Nombre priorizado
		"content":    content,
		"timestamp":  evt.Info.Timestamp.Unix(), // Int64 es mejor para bases de datos
		"is_from_me": evt.Info.IsFromMe,

		// Metadatos
		"message_type": getMessageType(evt),
	}
}

// Helper para logs
func getMessageType(evt *events.Message) string {
	msg := evt.Message
	if msg.GetConversation() != "" {
		return "text"
	}
	if msg.GetImageMessage() != nil {
		return "image"
	}
	if msg.GetVideoMessage() != nil {
		return "video"
	}
	if msg.GetAudioMessage() != nil {
		return "audio"
	}
	if msg.GetExtendedTextMessage() != nil {
		return "extended_text"
	}
	return "unknown"
}
