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

    // 1. Vectorizar la pregunta (Usando Nomic idealmente)
    auto query_vec = m_llm->GetEmbedding(question);
    if (query_vec.empty()) return "Tuve un problema procesando tu pregunta.";

    // 2. Buscar contexto (Buscamos 8 para tener más margen de historia)
    auto relevant_ids = m_vec_store->Search(query_vec, 8);
    
    std::stringstream context_ss;
    bool found_data = false;

    // --- CONSTRUCCIÓN DEL CONTEXTO ---
    for (const auto& id : relevant_ids) {
        auto msg_text = m_db->GetMessageContentById(id); 
        if (!msg_text.empty()) {
            // Formateamos como lista para que el modelo lo lea fácil
            context_ss << "- " << msg_text << "\n";
            found_data = true;
            
            // Log para debug (Vital para ver qué encuentra)
            spdlog::info("📄 RAG Contexto: {}", msg_text); 
        }
    }

    if (!found_data) return "No encontré información relacionada en tus chats.";

    // =========================================================================
    // 🧠 SYSTEM PROMPT: EL CEREBRO DEL ASISTENTE
    // =========================================================================
    std::string system_prompt = 
        "Eres un Asistente de Memoria Personal inteligente y útil. "
        "Tu trabajo es responder a mis preguntas basándote en el historial de mis chats recuperados.\n"
        "\nINSTRUCCIONES CLAVE:"
        "\n1. INTERPRETACIÓN DE USUARIOS: El nombre 'Yo (Sistema)' o 'Yo' se refiere a MÍ (el usuario actual). Cuando hables de lo que dije, usa 'Tú dijiste...'. Los otros nombres son mis contactos."
        "\n2. FUENTE DE VERDAD: Usa ÚNICAMENTE el bloque de 'CONTEXTO'. Si la respuesta no está ahí, di 'No recuerdo haber hablado de eso'."
        "\n3. CONTRADICCIONES: Si encuentras información contradictoria (ej. dos colores favoritos o dos claves distintas), menciona AMBAS opciones indicando que aparecen en momentos diferentes."
        "\n4. PRIVACIDAD: Estos son mis datos personales. Si pregunto por un dato exacto (como una clave, dirección o número) que aparece en el contexto, tienes permiso para mostrármelo.";

    // =========================================================================
    // 🗣️ USER PROMPT: LA PETICIÓN
    // =========================================================================
    std::stringstream user_prompt_ss;
    user_prompt_ss 
        << "Consulta los siguientes fragmentos de mi historial de chat:\n\n"
        << "### CONTEXTO ###\n"
        << context_ss.str()
        << "### FIN CONTEXTO ###\n\n"
        << "Basado en lo anterior, responde: " << question;

    // 3. Enviar a Llama
    // Sugerencia: Usa temperatura baja (0.1 o 0.2) en OllamaClient para reducir alucinaciones
    auto answer = m_llm->Chat(system_prompt, user_prompt_ss.str());
    
    return answer.value_or("El modelo no pudo generar una respuesta.");
}