// src/ipc/ipc_server.cpp
// logger.h? ?? include?? Windows SDK ?? ??
#include "logging/logger.h"
#include "ipc/ipc_server.h"
#include <chrono>
#include <sstream>
#include <random>
#include <iomanip>

namespace ipc {

IpcServer::IpcServer(core::DeviceManager& deviceManager)
    : pipeServer_(std::make_unique<NamedPipeServer>(PIPE_NAME))
    , deviceManager_(deviceManager) {
}

IpcServer::~IpcServer() {
    stop();
}

bool IpcServer::start() {
    if (!pipeServer_->start([this](PipeClient& client, const std::string& message) {
        handlePipeMessage(client, message);
    })) {
        logging::Logger::getInstance().error("Failed to start IPC server");
        return false;
    }
    
    logging::Logger::getInstance().info("IPC Server started successfully (Named Pipe: " + std::string(PIPE_NAME) + ")");
    return true;
}

void IpcServer::stop() {
    if (pipeServer_) {
        pipeServer_->stop();
    }
    
    logging::Logger::getInstance().info("IPC Server stopped");
}

void IpcServer::registerHandler(CommandType type, CommandHandler handler) {
    commandHandlers_[type] = handler;
}

void IpcServer::broadcastEvent(const Event& event) {
    logging::Logger::getInstance().info("IpcServer::broadcastEvent called - EventType: " + eventTypeToString(event.eventType));
    
    std::string json = MessageParser::serializeEvent(event);
    if (json.empty()) {
        logging::Logger::getInstance().error("Failed to serialize event to JSON");
        return;
    }
    
    logging::Logger::getInstance().debug("Event JSON: " + json);
    
    if (pipeServer_) {
        pipeServer_->broadcast(json);
        logging::Logger::getInstance().info("Event broadcasted via NamedPipeServer");
    } else {
        logging::Logger::getInstance().error("pipeServer_ is null, cannot broadcast event");
    }
}

void IpcServer::handlePipeMessage(PipeClient& client, const std::string& message) {
    try {
        if (message.empty()) {
            logging::Logger::getInstance().warn("Received empty message");
            return;
        }
        
        // Try to parse as Command
        auto command = MessageParser::parseCommand(message);
        if (!command) {
            logging::Logger::getInstance().warn("Failed to parse command message");
            
            // Send error response
            Response errorResp;
            errorResp.protocolVersion = PROTOCOL_VERSION;
            errorResp.kind = MessageKind::RESPONSE;
            errorResp.commandId = "";
            errorResp.status = ResponseStatus::FAILED;
            errorResp.timestampMs = std::chrono::duration_cast<std::chrono::milliseconds>(
                std::chrono::system_clock::now().time_since_epoch()).count();
            
            auto error = std::make_shared<Error>();
            error->code = "PARSE_ERROR";
            error->message = "Failed to parse command message";
            errorResp.error = error;
            
            std::string responseJson = MessageParser::serializeResponse(errorResp);
            pipeServer_->sendToClient(client, responseJson);
            return;
        }
        
        // Process command
        Response response = processCommand(*command);
        
        // Serialize and send response
        std::string responseJson = MessageParser::serializeResponse(response);
        if (responseJson.empty()) {
            logging::Logger::getInstance().error("Failed to serialize response");
            return;
        }
        
        pipeServer_->sendToClient(client, responseJson);
        
    } catch (const std::exception& e) {
        logging::Logger::getInstance().error("Error handling pipe message: " + std::string(e.what()));
    }
}

Response IpcServer::processCommand(const Command& command) {
    Response response;
    response.protocolVersion = command.protocolVersion;
    response.kind = MessageKind::RESPONSE;
    response.commandId = command.commandId;
    response.timestampMs = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::system_clock::now().time_since_epoch()).count();
    
    // Find handler
    auto it = commandHandlers_.find(command.type);
    if (it != commandHandlers_.end()) {
        try {
            response = it->second(command);
        } catch (const std::exception& e) {
            response.status = ResponseStatus::FAILED;
            auto error = std::make_shared<Error>();
            error->code = "HANDLER_ERROR";
            error->message = std::string(e.what());
            response.error = error;
        }
    } else {
        response.status = ResponseStatus::REJECTED;
        auto error = std::make_shared<Error>();
        error->code = "UNKNOWN_COMMAND";
        error->message = "Unknown command type: " + commandTypeToString(command.type);
        response.error = error;
    }
    
    return response;
}

} // namespace ipc
