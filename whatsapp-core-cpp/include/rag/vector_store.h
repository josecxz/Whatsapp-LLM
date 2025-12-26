#pragma once
#include <faiss/IndexFlat.h>
#include <vector>
#include <string>
#include <map>
#include <memory>

class VectorStore {
public:
    // Ajusta la dimensión según tu modelo (Qwen 0.5b suele ser 1024)
    VectorStore(int dimension = 1024);
    ~VectorStore();

    // Añade un vector asociado a un ID de mensaje de WhatsApp
    void AddIndex(const std::string& whatsapp_msg_id, const std::vector<float>& embedding);

    // Busca los IDs de WhatsApp más cercanos
    std::vector<std::string> Search(const std::vector<float>& query_embedding, int k = 5);

    // Persistencia básica
    void Save(const std::string& filepath);
    void Load(const std::string& filepath);

private:
    int m_dimension;
    std::unique_ptr<faiss::IndexFlatL2> m_index;
    
    // Mapa: ID_FAISS (long) -> ID_WHATSAPP (string)
    std::map<long, std::string> m_id_map;
    long m_current_faiss_id = 0;
};