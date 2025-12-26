#include "rag/vector_store.h"
#include <faiss/index_io.h>
#include <spdlog/spdlog.h>

VectorStore::VectorStore(int dimension) : m_dimension(dimension) {
    m_index = std::make_unique<faiss::IndexFlatL2>(dimension);
}

VectorStore::~VectorStore() = default;

void VectorStore::AddIndex(const std::string& whatsapp_msg_id, const std::vector<float>& embedding) {
    if (embedding.size() != m_dimension) {
        spdlog::warn("VectorStore: Dimensión incorrecta. Esperada {}, Recibida {}", m_dimension, embedding.size());
        return;
    }

    // FAISS acepta arrays crudos
    m_index->add(1, embedding.data());
    
    // Guardamos la relación ID
    m_id_map[m_current_faiss_id] = whatsapp_msg_id;
    m_current_faiss_id++;
}

std::vector<std::string> VectorStore::Search(const std::vector<float>& query_embedding, int k) {
    if (query_embedding.empty()) return {};

    std::vector<float> distances(k);
    std::vector<faiss::idx_t> labels(k);

    m_index->search(1, query_embedding.data(), k, distances.data(), labels.data());

    std::vector<std::string> results;
    for (int i = 0; i < k; ++i) {
        long found_id = labels[i];
        // -1 indica que no encontró suficientes vecinos
        if (found_id != -1 && m_id_map.count(found_id)) {
            results.push_back(m_id_map[found_id]);
        }
    }
    return results;
}

void VectorStore::Save(const std::string& filepath) {
    // Nota: Esto solo guarda el índice vectorial, no el mapa de IDs.
    // Para producción, deberías serializar m_id_map a un JSON también.
    faiss::write_index(m_index.get(), filepath.c_str());
}

void VectorStore::Load(const std::string& filepath) {
    // Carga básica
    faiss::Index* idx = faiss::read_index(filepath.c_str());
    if (auto flat_idx = dynamic_cast<faiss::IndexFlatL2*>(idx)) {
        m_index.reset(flat_idx);
    }
}