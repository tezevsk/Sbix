#include "Semantic.h"

#include <iostream>
#include <unordered_map>

#include "llvm/ADT/Hashing.h"
#include "llvm/ADT/StringExtras.h"

using Type = ExprNode::DataType;

struct SymbolInfo {
  Type type;
  bool isFunction = false;
  std::vector<Type> argumentTypes;
  std::shared_ptr<ExprNode::TypeMetadata> meta = nullptr;
};
std::unordered_map<std::string, SymbolInfo> symbolTable;

std::string stringPos(ASTNode* node) {
  return (std::to_string(node->loc.ln) + "!" + std::to_string(node->loc.col));
}

#include <string>
#include <string_view>

extern bool hasErrors;
extern unsigned int totalError;

inline void addErr() {
  hasErrors = true;
  totalError++;
}

bool hasEntry = false;

std::string Hash(std::string_view name) {
  llvm::hash_code hash = llvm::hash_value(name);
  std::string hex_hash = llvm::utohexstr(static_cast<uint64_t>(hash));
  return hex_hash;
}

bool AreTypesEqual(ExprNode::DataType typeA,
                   std::shared_ptr<ExprNode::TypeMetadata> metaA,
                   ExprNode::DataType typeB,
                   std::shared_ptr<ExprNode::TypeMetadata> metaB) {
  if (typeA != typeB) return false;
  if (typeA != ExprNode::DataType::Array &&
      typeA != ExprNode::DataType::Class) {
    return true;
  }
  if (typeA == ExprNode::DataType::Array) {
    if (!metaA || !metaB) return false;
    return AreTypesEqual(metaA->arrayElementBaseType, metaA->arrayElementMeta,
                         metaB->arrayElementBaseType, metaB->arrayElementMeta);
  }
  if (typeA == ExprNode::DataType::Class) {
    if (!metaA || !metaB) return false;
    return metaA->className == metaB->className;
  }

  return false;
}

auto FindVariableInScopes(const std::string& currentPrefix,
                          const std::string& name) {
  std::string tempPrefix = currentPrefix;

  while (true) {
    auto it = symbolTable.find(tempPrefix + name);
    if (it != symbolTable.end()) {
      return it;
    }

    if (tempPrefix.empty())
      break;
    tempPrefix.pop_back();
    size_t lastColon = tempPrefix.rfind(':');
    if (lastColon == std::string::npos) {
      tempPrefix = "";
    } else {
      tempPrefix = tempPrefix.substr(0, lastColon + 1);
    }
  }
  return symbolTable.end();
}

std::string stringType(Type type,
                       std::shared_ptr<ExprNode::TypeMetadata> meta = nullptr) {
  switch (type) {
    case Type::Int:
      return "Int";
    case Type::Bool:
      return "Bool";
    case Type::Float:
      return "Float";
    case Type::String:
      return "String";
    case Type::Unknown:
      return "Void";
    case Type::Array:
      if (!meta) return "Collection";
      return "Collection<" +
             stringType(meta->arrayElementBaseType, meta->arrayElementMeta);
    case Type::Class:
      if (!meta || !meta->className.empty()) return "Class";
      return meta->className;
    default:
      return "";
  }
}

int loop_depth = 0;

void Analyze(ASTNode* node, std::string addPrefix = "") {
  if (!node) return;
  if (node->getType() == NodeType::constant)
    return;

  if (node->getType() == NodeType::binnaryOp) {
    auto& binNode = static_cast<BinnaryOpNode&>(*node);
    Analyze(binNode.left.get(), addPrefix);
    Analyze(binNode.right.get(), addPrefix);
    Type leftType = binNode.left->evaluatedType;
    Type rightType = binNode.right->evaluatedType;
    if (leftType == Type::Array && rightType == Type::Array) {
      std::cerr << "[\033[31mSBX-S152\033[0m] Invalid operation: cannot use "
                   "'+' on arrays.\n"
                   "-> If you wanted to merge collections, use '.extend()'.\n"
                   "-> If you wanted element-wise addition, use pattern "
                   "matching or a loop.\n"
                   "-> Otherwise, please double-check your variable names.\n";
      addErr();
      return;
    }
    if (leftType == Type::Unknown &&
        (rightType == Type::Int || rightType == Type::Float)) {
      leftType = rightType;
      binNode.left->evaluatedType = rightType;
      if (binNode.left->getType() == NodeType::variableUse) {
        auto& varNode = static_cast<VariableNode&>(*binNode.left);
        auto it = FindVariableInScopes(addPrefix, varNode.name);
        if (it != symbolTable.end()) it->second.type = rightType;
      }
    }
    if (!(((leftType == Type::Float || leftType == Type::Int) &&
           (rightType == Type::Float || rightType == Type::Int)) ||
          leftType == rightType)) {
      std::cerr
          << "[\033[31mSBX-S151\033[0m] Incompatable type operation atempt. ("
          << node->loc.ln << ":" << node->loc.col << ")\n";
      addErr();
    }
    if (leftType == Type::Float || rightType == Type::Float) {
      binNode.evaluatedType = Type::Float;
    }
    if (binNode.op == Operator::greaterThan ||
        binNode.op == Operator::lessThan || binNode.op == Operator::equal ||
        binNode.op == Operator::notEqual) {
      binNode.evaluatedType = Type::Bool;
    } else {
      binNode.evaluatedType = leftType;
    }
  }
  if (node->getType() == NodeType::namespace_) {
		auto& namesp = static_cast<Namespace&>(*node);
		for (const auto& insideBlock : namesp.nrr) {
			Analyze(insideBlock.get(), addPrefix + namesp.np + "_");
		}
	}
  if (node->getType() == NodeType::collection) {
    auto collection = dynamic_cast<CollectionNode*>(node);
    if (collection) {
      collection->evaluatedType = ExprNode::DataType::Array;
      collection->typeMeta = std::make_shared<ExprNode::TypeMetadata>();
      if (!collection->objects.empty()) {
        collection->typeMeta->arrayElementBaseType =
            collection->objects[0]->evaluatedType;
        collection->typeMeta->arrayElementMeta =
            collection->objects[0]->typeMeta;
      } else {
        collection->typeMeta->arrayElementBaseType =
            ExprNode::DataType::Unknown;
        collection->typeMeta->arrayElementMeta = nullptr;
      }
    }
  }

  if (node->getType() == NodeType::collectionOverride) {
    auto& collection = static_cast<CollectionOverrideNode&>(*node);

    auto it = symbolTable.find(collection.name);
    if (it == symbolTable.end()) {
      std::cerr
          << "[\033[31mSBX-S102\033[0m] Assignment to undeclared variable: "
          << collection.name << " (" << node->loc.ln << ":" << node->loc.col
          << ")\n";
      return;
    }

    const SymbolInfo& symbol = it->second;
    if (symbol.type != ExprNode::DataType::Array) {
      std::cerr << "[\033[31mSBX-S104\033[0m] Variable isn't collection: "
                << collection.name << " (" << node->loc.ln << ":"
                << node->loc.col << ")\n";
      addErr();
      return;
    }
    if (collection.content) {
      if (symbol.meta && symbol.meta->arrayElementBaseType !=
                             collection.content->evaluatedType) {
        std::cerr
            << "[\033[31mSBX-S104\033[0m] Type Mismatch in assignment to '"
            << collection.name << "'. (" << node->loc.ln << ":" << node->loc.col
            << ")\n";
        addErr();
      }
    }
  }

  if (node->getType() == NodeType::variableDeclr) {
    auto& varNode = static_cast<VariableDeclrNode&>(*node);
    // 捷泽夫斯克
    if (symbolTable.find(addPrefix + varNode.name) != symbolTable.end()) {
      std::cerr << "[\033[31mSBX-S103\033[0m] Variable redefenition. Name: "
                << varNode.name << "(" << node->loc.ln << ":" << node->loc.col
                << ")\n";
      addErr();
      return;
    }
    Analyze(varNode.expression.get(), addPrefix);
    varNode.mangledName = addPrefix + varNode.name;
    symbolTable.insert(
        {addPrefix + varNode.name, {varNode.expression->evaluatedType}});
  }
  if (node->getType() == NodeType::variableUse) {
    auto& varNode = static_cast<VariableNode&>(*node);
    auto it = FindVariableInScopes(addPrefix, varNode.name);
    if (it == symbolTable.end()) {
      std::cerr << "[\033[31mSBX-S101\033[0m] Use of undeclared variable: "
                << varNode.name << " (" << node->loc.ln << ":" << node->loc.col
                << ")\n";
      addErr();
      return;
    }
    varNode.mangledName = it->first;
    varNode.evaluatedType = it->second.type;
  }
  if (node->getType() == NodeType::variableOverride) {
    auto& varNode = static_cast<VariableAssignmentNode&>(*node);
    auto it = FindVariableInScopes(addPrefix, varNode.name);
    if (it == symbolTable.end()) {
      std::cerr
          << "[\033[31mSBX-S102\033[0m] Assignment to undeclared variable: "
          << varNode.name << " (" << node->loc.ln << ":" << node->loc.col
          << ")\n";
      addErr();
      return;
    }

    Analyze(varNode.expression.get(), addPrefix);

    Type varType = it->second.type;
    Type valueType = varNode.expression->evaluatedType;

    if (!(((varType == Type::Float || varType == Type::Int) &&
           (valueType == Type::Float || valueType == Type::Int)) ||
          varType == valueType)) {
      std::cerr << "[\033[31mSBX-S104\033[0m] Type mismatch in assignment to '"
                << varNode.name << "'. (" << node->loc.ln << ":"
                << node->loc.col << ")\n";
      addErr();
    }
  }
  if (node->getType() == NodeType::functionDef) {
    auto& funcNode = static_cast<FunctionDeclNode&>(*node);

    if (symbolTable.find(addPrefix + funcNode.functionName) !=
        symbolTable.end()) {
      std::cerr << "[\033[31mSBX-S203\033[0m] Function redefinition. Name: "
                << funcNode.functionName << "(" << node->loc.ln << ":"
                << node->loc.col << ")\n";
      addErr();
      return;
    }
    std::vector<Type> initialArgTypes;
    if (!funcNode.isExtern) {
      funcNode.mangledName = addPrefix + funcNode.functionName;

      std::string nextPrefix = addPrefix + Hash(funcNode.functionName) + ":";
      for (auto& paramName : funcNode.Params) {
        if (paramName.name.empty()) continue;
        symbolTable.insert(
            {nextPrefix + paramName.name, {paramName.type, false, {}}});
        initialArgTypes.push_back(paramName.type);
        paramName.mangledName = nextPrefix + paramName.name;
      }
      symbolTable.insert({addPrefix + funcNode.functionName,
                          {Type::Unknown, true, initialArgTypes}});
      Analyze(funcNode.does.get(), nextPrefix);

      auto retIt = symbolTable.find(addPrefix + funcNode.functionName);
      if (retIt != symbolTable.end()) {
        funcNode.evaluatedType = retIt->second.type;
      }
    } else {
      funcNode.mangledName = funcNode.functionName;
      symbolTable.insert({funcNode.functionName,
                          {funcNode.evaluatedType, true, initialArgTypes}});
    }
  }
  if (node->getType() == NodeType::classn) {
    auto& classNode = static_cast<ClassNode&>(*node);
    std::string nextPrefix = addPrefix + classNode.name + '_';
    for (auto& n : classNode._public) {
      Analyze(n.get(), nextPrefix);
    }
    for (auto& n : classNode._private) {
      Analyze(n.get(), nextPrefix);
    }
  }
  if (node->getType() == NodeType::methodCall) {
    auto& method = static_cast<MethodCallNode&>(*node);
    std::string className = "";
    auto it = FindVariableInScopes(addPrefix, method.functionName);
    if (!method.object) {
      std::cerr << "[\033[31mError\033[0m] Non-static method call missing "
                   "object target ("
                << node->loc.ln << ":" << node->loc.col << ")\n";
      addErr();
      return;
    }
    if (it == symbolTable.end()) {
      std::cerr << "[\033[31mSBX-S206\033[0m] Call to undeclared class: "
                << method.functionName << " (" << node->loc.ln << ":"
                << node->loc.col << ")\n";
      addErr();
      return;
    }
    if (!method.isStatic && method.object) {
      Analyze(method.object.get(), addPrefix);
    } else if (method.isStatic) {
    }
  }

  if (node->getType() == NodeType::returns) {
    auto& returnNode = static_cast<ReturnNode&>(*node);
    Analyze(returnNode.returns.get(), addPrefix);
    returnNode.evaluatedType = returnNode.returns->evaluatedType;

    std::string tempPrefix = addPrefix;
    if (!tempPrefix.empty() && tempPrefix.back() == ':') {
      tempPrefix.pop_back();
      size_t lastColon = tempPrefix.rfind(':');
      std::string funcHash = (lastColon == std::string::npos)
                                 ? tempPrefix
                                 : tempPrefix.substr(lastColon + 1);

      for (auto& [key, info] : symbolTable) {
        if (info.isFunction) {
          size_t namePos = key.rfind(':');
          std::string rawName =
              (namePos == std::string::npos) ? key : key.substr(namePos + 1);
          if (Hash(rawName) == funcHash) {
            info.type =
                returnNode.evaluatedType;
            break;
          }
        }
      }
    }
  }

  if (node->getType() == NodeType::functionCall) {
    auto& callNode = static_cast<FunctionCallNode&>(*node);
    for (const auto& arg : callNode.Args) {
      Analyze(arg.get(), addPrefix);
    }
    auto it = FindVariableInScopes(addPrefix, callNode.functionName);
    if (it == symbolTable.end()) {
      std::cerr << "[\033[31mSBX-S202\033[0m] Call to undeclared function: "
                << callNode.functionName << " (" << node->loc.ln << ":"
                << node->loc.col << ")\n";
      callNode.evaluatedType =
          Type::Unknown;
      addErr();
      return;
    }
    const auto& expectedTypes = it->second.argumentTypes;
    if (callNode.Args.size() != it->second.argumentTypes.size()) {
      std::cerr << "[\033[31mSBX-S204\033[0m] Function '"
                << callNode.functionName << "' expects "
                << it->second.argumentTypes.size() << " arguments, but "
                << callNode.Args.size() << " was provided.\n";
      addErr();
    } else {
      for (size_t i = 0; i < callNode.Args.size(); ++i) {
        Type providedType = callNode.Args[i]->evaluatedType;
        Type expectedType = expectedTypes[i];

        if (providedType != expectedType) {

          std::cerr << "[\033[31mSBX-S205\033[0m] Type mismatch in function '"
                    << callNode.functionName << "' at argument " << (i + 1)
                    << ". Expected '" << stringType(expectedType)
                    << "', but got '" << stringType(providedType) << "'.\n";
          addErr();
        }
      }
    }
    
    callNode.mangledName = it->first;
    callNode.evaluatedType = it->second.type;
  }

  if (node->getType() == NodeType::ifStatement) {
    auto& ifNode = static_cast<IfStatementNode&>(*node);

    Analyze(ifNode.condition.get(), addPrefix);
    if (ifNode.condition->evaluatedType != Type::Bool) {
      std::cerr << "[\033[31mSBX-S153\033[0m] Condition must be Boolean. ("
                << node->loc.ln << ":" << node->loc.col << ")\n";
      addErr();
    }

    std::string nextPrefix = addPrefix + Hash("if" + stringPos(node)) + ":";
    Analyze(ifNode.thenBlock.get(), nextPrefix);
    if (ifNode.orelse) {
      Analyze(ifNode.orelse.get(), nextPrefix + "else:");
    }
  }
  if (node->getType() == NodeType::whileStatement) {
    auto& whileNode = static_cast<WhileStatementNode&>(*node);

    Analyze(whileNode.condition.get(), addPrefix);
    if (whileNode.condition->evaluatedType != Type::Bool) {
      std::cerr << "[\033[31mSBX-S153\033[0m] Loop condition must be Boolean. ("
                << node->loc.ln << ":" << node->loc.col << ")\n";
      addErr();
    }

    std::string nextPrefix = addPrefix + Hash("while" + stringPos(node)) + ":";
    loop_depth++;
    Analyze(whileNode.body.get(), nextPrefix);
    loop_depth--;
  }
  if (node->getType() == NodeType::forStatement) {
    auto& forNode = static_cast<ForStatementNode&>(*node);

    std::string nextPrefix = addPrefix + Hash("for" + stringPos(node)) + ":";

    forNode.mangledName = nextPrefix + forNode.varName;
    symbolTable.insert({forNode.mangledName, {Type::Int}});

    loop_depth++;
    Analyze(forNode.body.get(), nextPrefix);
    loop_depth--;
  }
  if (node->getType() == NodeType::block) {
    auto& blockNode = static_cast<Block&>(*node);
    for (const auto& insideBlock : blockNode.block) {
      Analyze(insideBlock.get(), addPrefix);
    }
  }
  if (node->getType() == NodeType::break_) {
    if (loop_depth <= 0) {
      std::cerr
          << "[\033[31mSBX-SBRK\033[0m] 'break' statement outside of a loop. ("
          << node->loc.ln << ":" << node->loc.col << ")\n";
      hasErrors = true;
      totalError++;
    }
  }
  if (node->getType() == NodeType::main) {
    if (hasEntry) {
      std::cerr << "[\033[31mSBX-S401\033[0m] Multiple entry points defined. ("
                << node->loc.ln << ":" << node->loc.col << ")\n";
      hasErrors = true;
      totalError++;
    }
    hasEntry = true;
    auto& mainNode = static_cast<MainNode&>(*node);
    if (mainNode.logic.empty()) {
      std::cerr
          << "[\033[31mSBX-S402\033[0m] Entry defined, but can't be empty. ("
          << node->loc.ln << ":" << node->loc.col << ")\n";
      hasErrors = true;
      totalError++;
    }
  }
}

void Semantic(NodeArray& nrr) {
  symbolTable.clear();
  for (const auto& node : nrr) {
    Analyze(node.get());
  }
}
