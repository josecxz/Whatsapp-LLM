#pragma once

#include <string>
#include <vector>
#include <memory>
#include <optional>
#include <mutex>

// Forward declarations para no incluir todos los headers aquí y compilar más rápido
class OllamaClient;
class VectorStore;
class Repository; // Asumo que tu clase de DB se llama Repository o MessageDatabase

class RagService {
public:
    RagService(std::shared_ptr<OllamaClient> llm, 
               std::shared_ptr<VectorStore> v_store,
               std::shared_ptr<Repository> db);

    // 1. INGESTIÓN: Procesa un mensaje nuevo (Lo guarda en FAISS)
    // Se llama automáticamente cuando llega un mensaje de WhatsApp
    void IngestMessage(const std::string& msg_id, const std::string& content, const std::string& sender);

    // 2. CONSULTA: El usuario hace una pregunta y respondemos con datos
    std::string Ask(const std::string& question);

    void LoadHistoryFromDB();

private:
    std::shared_ptr<OllamaClient> m_llm;
    std::shared_ptr<VectorStore> m_vec_store;
    std::shared_ptr<Repository> m_db;
    std::mutex m_ingest_mutex;
};