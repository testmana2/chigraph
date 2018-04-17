/// \file NodeInstance.cpp

#include "chi/NodeInstance.hpp"
#include "chi/DataType.hpp"
#include "chi/FunctionValidator.hpp"
#include "chi/GraphFunction.hpp"
#include "chi/GraphModule.hpp"
#include "chi/NodeType.hpp"
#include "chi/Support/Result.hpp"

#include <cassert>

namespace chi {
NodeInstance::NodeInstance(GraphFunction* func, std::unique_ptr<NodeType> nodeType, float posX,
                           float posY, boost::uuids::uuid nodeID)
    : mType{std::move(nodeType)},
      mX{posX},
      mY{posY},
      mId{nodeID},
      mContext{&mType->context()},
      mFunction{func},
      mGraphModule{&func->module()} {
	assert(mType != nullptr && mFunction != nullptr);

	mType->mNodeInstance = this;

	inputDataConnections.resize(type().dataInputs().size(), {nullptr, ~0ull});
	outputDataConnections.resize(type().dataOutputs().size(), {});

	if (!type().pure()) {
		inputExecConnections.resize(type().execInputs().size(), {});
		outputExecConnections.resize(type().execOutputs().size(), {nullptr, ~0ull});
	}
}

NodeInstance::NodeInstance(const NodeInstance& other, boost::uuids::uuid id)
    : mType(other.type().clone()),
      mX{other.x()},
      mY{other.y()},
      mId{id},
      mContext{&other.context()},
      mFunction{&other.function()} {
	assert(mType != nullptr && mFunction != nullptr);

	mType->mNodeInstance = this;

	inputDataConnections.resize(type().dataInputs().size(), {nullptr, ~0ull});
	outputDataConnections.resize(type().dataOutputs().size(), {});

	inputExecConnections.resize(type().execInputs().size(), {});
	outputExecConnections.resize(type().execOutputs().size(), {nullptr, ~0ull});
}

NodeInstance::~NodeInstance() = default;

void NodeInstance::setType(std::unique_ptr<NodeType> newType) {
	module().updateLastEditTime();

	// delete exec connections that are out of range
	// start at one past the end
	for (size_t id = newType->execInputs().size(); id < inputExecConnections.size(); ++id) {
		while (!inputExecConnections[id].empty()) {
			assert(inputExecConnections[id][0].first);  // should never fail...
			auto res = disconnectExec(*inputExecConnections[id][0].first,
			                          inputExecConnections[id][0].second);

			assert(res.mSuccess);
		}
	}
	inputExecConnections.resize(newType->execInputs().size());

	for (size_t id = newType->execOutputs().size(); id < outputExecConnections.size(); ++id) {
		auto& conn = outputExecConnections[id];
		if (conn.first != nullptr) { disconnectExec(*this, id); }
	}
	outputExecConnections.resize(newType->execOutputs().size(), std::make_pair(nullptr, ~0ull));

	auto id = 0ull;
	for (const auto& conn : inputDataConnections) {
		// if there is no connection, then skip
		if (conn.first == nullptr) {
			++id;
			continue;
		}

		// if we get here, then we have to deal with the connection.

		// see if we can keep it
		if (newType->dataInputs().size() > id &&
		    type().dataInputs()[id].type == newType->dataInputs()[id].type) {
			++id;
			continue;
		}

		// disconnect if there's a connection and we can't keep it
		auto res = disconnectData(*conn.first, conn.second, *this);

		assert(res.mSuccess);

		++id;
	}
	inputDataConnections.resize(newType->dataInputs().size(), std::make_pair(nullptr, ~0ull));

	id = 0ull;
	for (const auto& connSlot : outputDataConnections) {
		// keep the connections if they're still good
		if (newType->dataOutputs().size() > id &&
		    type().dataOutputs()[id].type == newType->dataOutputs()[id].type) {
			++id;
			continue;
		}

		while (!connSlot.empty()) {
			assert(connSlot[0].first);

			auto res = disconnectData(*this, id, *connSlot[0].first);

			assert(res.mSuccess);
		}
		++id;
	}
	outputDataConnections.resize(newType->dataOutputs().size());

	mType                = std::move(newType);
	mType->mNodeInstance = this;
}

Result connectData(NodeInstance& lhs, size_t lhsConnID, NodeInstance& rhs, size_t rhsConnID) {
	Result res = {};

	assert(&lhs.function() == &rhs.function());

	rhs.module().updateLastEditTime();

	// make sure the connection exists
	// the input to the connection is the output to the node
	if (lhsConnID >= lhs.outputDataConnections.size()) {
		auto dataOutputs = nlohmann::json::array();
		for (auto& output : lhs.type().dataOutputs()) {
			dataOutputs.push_back({{output.name, output.type.qualifiedName()}});
		}

		res.addEntry("E22", "Output Data connection doesn't exist in node",
		             {{"Requested ID", lhsConnID},
		              {"Node Type", lhs.type().qualifiedName()},
		              {"Node JSON", rhs.type().toJSON()},
		              {"Node Output Data Connections", dataOutputs}});
	}
	if (rhsConnID >= rhs.inputDataConnections.size()) {
		auto dataInputs = nlohmann::json::array();
		for (auto& output : rhs.type().dataInputs()) {
			dataInputs.push_back({{output.name, output.type.qualifiedName()}});
		}

		res.addEntry("E23", "Input Data connection doesn't exist in node",
		             {{"Requested ID", rhsConnID},
		              {"Node Type", rhs.type().qualifiedName()},
		              {"Node JSON", rhs.type().toJSON()},
		              {"Node Input Data Connections", dataInputs}});
	}

	// if there are errors, back out
	if (!res) { return res; }
	// make sure the connection is of the right type
	if (lhs.type().dataOutputs()[lhsConnID].type != rhs.type().dataInputs()[rhsConnID].type) {
		res.addEntry("E24", "Connecting data nodes with different types is invalid",
		             {{"Left Hand Type", lhs.type().dataOutputs()[lhsConnID].type.qualifiedName()},
		              {"Right Hand Type", rhs.type().dataInputs()[rhsConnID].type.qualifiedName()},
		              {"Left Node JSON", rhs.type().toJSON()},
		              {"Right Node JSON", rhs.type().toJSON()}});
		return res;
	}

	// if we are replacing a connection, disconnect it
	if (rhs.inputDataConnections[rhsConnID].first != nullptr) {
		res += disconnectData(lhs, lhsConnID, rhs);
		if (!res) { return res; }
	}

	lhs.outputDataConnections[lhsConnID].emplace_back(&rhs, rhsConnID);
	rhs.inputDataConnections[rhsConnID] = {&lhs, lhsConnID};

	return res;
}

Result connectExec(NodeInstance& lhs, size_t lhsConnID, NodeInstance& rhs, size_t rhsConnID) {
	Result res = {};

	assert(&lhs.function() == &rhs.function());

	lhs.module().updateLastEditTime();

	// make sure the connection exists
	if (lhsConnID >= lhs.outputExecConnections.size()) {
		auto execOutputs = nlohmann::json::array();
		for (auto& output : lhs.type().execOutputs()) { execOutputs.push_back(output); }

		res.addEntry("E22", "Output exec connection doesn't exist in node",
		             {{"Requested ID", lhsConnID},
		              {"Node Type", lhs.type().qualifiedName()},
		              {"Node Output Exec Connections", execOutputs}});
	}
	if (rhsConnID >= rhs.inputExecConnections.size()) {
		auto execInputs = nlohmann::json::array();
		for (auto& output : rhs.type().execInputs()) { execInputs.push_back(output); }

		res.addEntry("E23", "Input exec connection doesn't exist in node",
		             {{"Requested ID", lhsConnID},
		              {"Node Type", rhs.type().qualifiedName()},
		              {"Node Input Exec Connections", execInputs}

		             });
	}

	if (!res) { return res; }
	// if we are replacing a connection, disconnect it
	if (lhs.outputExecConnections[lhsConnID].first != nullptr) {
		res += disconnectExec(lhs, lhsConnID);
		if (!res) { return res; }
	}

	// connect it!
	lhs.outputExecConnections[lhsConnID] = {&rhs, rhsConnID};
	rhs.inputExecConnections[rhsConnID].emplace_back(&lhs, lhsConnID);

	return res;
}

Result disconnectData(NodeInstance& lhs, size_t lhsConnID, NodeInstance& rhs) {
	assert(&lhs.function() == &rhs.function());

	lhs.module().updateLastEditTime();

	Result res = {};

	if (lhsConnID >= lhs.outputDataConnections.size()) {
		auto dataOutputs = nlohmann::json::array();
		for (auto& output : lhs.type().dataOutputs()) {
			dataOutputs.push_back({{output.name, output.type.qualifiedName()}});
		}

		res.addEntry("E22", "Output data connection in node doesn't exist",
		             {{"Requested ID", lhsConnID},
		              {"Node Type", lhs.type().qualifiedName()},
		              {"Node JSON", rhs.type().toJSON()},
		              {"Node Output Data Connections", dataOutputs}});

		return res;
	}

	// find the connection
	auto iter = std::find_if(lhs.outputDataConnections[lhsConnID].begin(),
	                         lhs.outputDataConnections[lhsConnID].end(),
	                         [&](auto& pair) { return pair.first == &rhs; });

	if (iter == lhs.outputDataConnections[lhsConnID].end()) {
		res.addEntry("EUKN", "Cannot disconnect from connection that doesn't exist",
		             {{"Left node ID", lhs.stringId()},
		              {"Right node ID", rhs.stringId()},
		              {"Left dock ID", lhsConnID}});

		return res;
	}

	if (rhs.inputDataConnections.size() <= iter->second) {
		auto dataInputs = nlohmann::json::array();
		for (auto& output : rhs.type().dataInputs()) {
			dataInputs.push_back({{output.name, output.type.qualifiedName()}});
		}

		res.addEntry("E23", "Input Data connection doesn't exist in node",
		             {{"Requested ID", iter->second},
		              {"Node Type", rhs.type().qualifiedName()},
		              {"Node JSON", rhs.type().toJSON()},
		              {"Node Input Data Connections", dataInputs}});

		return res;
	}

	if (rhs.inputDataConnections[iter->second] != std::make_pair(&lhs, lhsConnID)) {
		res.addEntry("EUKN", "Cannot disconnect from connection that doesn't exist",
		             {{"Left node ID", lhs.stringId()}, {"Right node ID", rhs.stringId()}});

		return res;
	}

	// finally actually disconnect it
	rhs.inputDataConnections[iter->second] = {nullptr, ~0ull};
	lhs.outputDataConnections[lhsConnID].erase(iter);

	return res;
}

Result disconnectExec(NodeInstance& lhs, size_t lhsConnID) {
	Result res = {};

	lhs.module().updateLastEditTime();

	if (lhsConnID >= lhs.outputExecConnections.size()) {
		auto execOutputs = nlohmann::json::array();
		for (auto& output : lhs.type().execOutputs()) { execOutputs.push_back(output); }

		res.addEntry("E22", "Output exec connection doesn't exist in node",
		             {{"Requested ID", lhsConnID},
		              {"Node Type", lhs.type().qualifiedName()},
		              {"Node Output Exec Connections", execOutputs}});
	}

	auto& lhsconn  = lhs.outputExecConnections[lhsConnID];
	auto& rhsconns = lhsconn.first->inputExecConnections[lhsconn.second];

	auto iter = std::find_if(rhsconns.begin(), rhsconns.end(),
	                         [&](auto pair) { return pair == std::make_pair(&lhs, lhsConnID); });

	if (iter == rhsconns.end()) {
		res.addEntry("EUKN", "Cannot disconnect an exec connection that doesn't connect back",
		             {{"Left node ID", lhs.stringId()}, {"Left node dock id", lhsConnID}});
		return res;
	}

	rhsconns.erase(iter);
	lhsconn = {nullptr, ~0ull};

	return res;
}

}  // namespace chi
