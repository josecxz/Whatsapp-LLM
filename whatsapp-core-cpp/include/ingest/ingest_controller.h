#pragma once
#include "cpp-httplib/httplib.h"
#include <memory>
// Incluimos el repositorio para poder usar la base de datos
#include "persistence/repository.h" 

class RagService; 

class IngestController {
public:
    // ACTUALIZADO: El constructor ahora pide RAG + Base de Datos
    IngestController(std::shared_ptr<RagService> rag_service, std::shared_ptr<Repository> db);

    void RegisterRoutes(httplib::Server& server);

private:
    std::shared_ptr<RagService> m_rag_service;
    
    // NUEVO: Variable para guardar la conexión a base de datos
    std::shared_ptr<Repository> m_db; 
};