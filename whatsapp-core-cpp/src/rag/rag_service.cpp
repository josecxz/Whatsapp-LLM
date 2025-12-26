#include "rag/rag_service.h"
#include "llm/ollama_client.h"
#include "rag/vector_store.h"
#include "persistence/repository.h"
#include <spdlog/spdlog.h>
#include <sstream>

void RagService::LoadHistoryFromDB() {
    spdlog::info("⏳ Iniciando carga de historial en RAG (Esto puede tardar)...");
    
    // 1. Pedimos los últimos 50 mensajes (puedes cambiar el número)
    // Si pones muchos, Ollama tardará bastante en generar los embeddings.
    auto history = m_db->GetAllMessages(1000); 

    int count = 0;
    int total = history.size();

    for (const auto& msg : history) {
        count++;
        // Usamos spdlog para mostrar progreso en la misma línea si es posible, o log normal
        if (count % 5 == 0) spdlog::info("PROGRESO: Indexando {}/{}", count, total);

        // Llamamos a nuestra propia función de ingesta.
        // Como IngestMessage tiene mutex, es seguro.
        IngestMessage(msg.id, msg.content, msg.sender);
    }

    spdlog::info("✅ Carga de historial completada. {} mensajes listos en memoria.", total);
}
// Constructor
RagService::RagService(std::shared_ptr<OllamaClient> llm, 
                       std::shared_ptr<VectorStore> v_store,
                       std::shared_ptr<Repository> db)
    : m_llm(llm), m_vec_store(v_store), m_db(db) {}

void RagService::IngestMessage(const std::string& msg_id, const std::string& content, const std::string& sender) {
    
    // =================================================================================
    // 🔒 CANDADO DE SEGURIDAD (MUTEX)
    // =================================================================================
    // Esto obliga a que los mensajes se procesen UNO A UNO, aunque lleguen 100 a la vez.
    // Evita que FAISS explote por concurrencia y evita timeouts de Ollama.
    std::lock_guard<std::mutex> lock(m_ingest_mutex);
    // =================================================================================

    // 1. Validar limpieza (ignorar mensajes muy cortos)
    if (content.length() < 2) return;

    // 2. Crear texto enriquecido
    std::string text_to_embed = sender + ": " + content;

    // 3. Obtener vector de Ollama (Esto puede tardar 0.2s - 1s)
    auto embedding = m_llm->GetEmbedding(text_to_embed);

    // 4. Guardar en FAISS
    if (!embedding.empty()) {
        // NOTA: El orden suele ser (Vector, ID). Si tu VectorStore está al revés, cámbialo aquí.
        m_vec_store->AddIndex(msg_id, embedding); 
        spdlog::info("🧠 Mensaje indexado en RAG: {}", msg_id);
    } else {
        spdlog::warn("⚠️ Fallo al generar embedding para mensaje: {}", msg_id);
    }
}

std::string RagService::Ask(const std::string& question) {
    spdlog::info("🤖 Usuario pregunta: {}", question);

    // PASO A: Vectorizar la pregunta
    auto query_vec = m_llm->GetEmbedding(question);
    if (query_vec.empty()) {
        return "Lo siento, tuve un error interno procesando tu pregunta (Embedding error).";
    }

    // PASO B: Buscar los 5 mensajes más relevantes en FAISS
    std::vector<std::string> relevant_ids = m_vec_store->Search(query_vec, 5);

    spdlog::info("🔍 RAG encontró {} IDs relevantes.", relevant_ids.size());

    if (relevant_ids.empty()) {
        return "No encontré información relevante en tus chats anteriores para responder a eso.";
    }

    // PASO C: Recuperar el texto real de MariaDB usando los IDs
    std::stringstream context_ss;
    context_ss << "Historial de mensajes recuperado:\n";
    
    for (const auto& id : relevant_ids) {
        auto msg_text = m_db->GetMessageContentById(id); 
        if (!msg_text.empty()) {
            context_ss << "- " << msg_text << "\n";
            spdlog::info("   -> Contexto recuperado: {}", msg_text); // Log para depurar
        }
    }

    // PASO D: Construir el Prompt para el LLM
    std::string context = context_ss.str();
    
    // Si la DB falló y no trajo texto, no enviamos basura a la IA
    if (context.length() < 35) { // 35 es aprox el largo del header "Historial..."
        return "Encontré referencias a mensajes antiguos, pero no pude leer su contenido de la base de datos.";
    }

    std::string system_prompt = 
        "Eres un asistente personal inteligente que analiza conversaciones de WhatsApp. "
        "Tu objetivo es responder a la pregunta del usuario basándote EXCLUSIVAMENTE en el contexto proporcionado abajo. "
        "Si la respuesta no está en el contexto, di amablemente que no tienes esa información. "
        "Sé conciso y directo.\n\n"
        "CONTEXTO:\n" + context;

    // PASO E: Generar respuesta
    auto answer = m_llm->Chat(system_prompt, question);
    
    return answer.value_or("Error conectando con el modelo de IA.");
}