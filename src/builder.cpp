#include "builder.h"

#include <iostream>
#include <map>
#include <memory>
#include <sstream>
#include <string>
#include <vector>

struct TranspileContext {
  std::stringstream body;
  std::stringstream headers;
  std::stringstream structs;
  std::stringstream prototypes;
  int indentLevel = 0;

  std::vector<std::vector<std::string>> allocatedStrings = {{}};

  void enterBlock() { allocatedStrings.push_back({}); }
  void leaveBlock() { allocatedStrings.pop_back(); }
  void registerString(const std::string& name) {
    allocatedStrings.back().push_back(name);
  }

  void emitIndent() { body << std::string(indentLevel * 4, ' '); }
};
std::string toCType(ExprNode::DataType type) {
  switch (type) {
    using enum ExprNode::DataType;
    case Int:
      return "int32_t";
    case Float:
      return "double";
    case Bool:
      return "bool";
    case String:
      return "StringView";
    default:
      return "void";
  }
}
const std::map<Operator, std::string> OperatorMap = {
    {Operator::plus, "+"},         {Operator::minus, "-"},
    {Operator::multiply, "*"},     {Operator::divide, "/"},
    {Operator::lessThan, "<"},     {Operator::greaterThan, ">"},
    {Operator::equal, "=="},       {Operator::notEqual, "!="},
    {Operator::lessOrEqual, "<="}, {Operator::greaterOrEqual, ">="}};
std::string transpileExpr(ASTNode* node, TranspileContext& ctx);
void transpileStatement(ASTNode* node, TranspileContext& ctx);
std::string transpileExpr(ASTNode* node, TranspileContext& ctx) {
  if (!node) return "0";

  switch (node->getType()) {
    case NodeType::constant: {
      auto& lit = static_cast<LiteralNode&>(*node);
      if (lit.evaluatedType == ExprNode::DataType::Int) return lit.value;
      if (lit.evaluatedType == ExprNode::DataType::Float) return lit.value;
      if (lit.evaluatedType == ExprNode::DataType::Bool)
        return (lit.value == "true") ? "true" : "false";

      if (lit.evaluatedType == ExprNode::DataType::String) {
        std::string lenStr = std::to_string(lit.value.length());
        return "(StringView){ .data = (char*)memcpy(malloc(" + lenStr +
               " + 1), \"" + lit.value + "\", " + lenStr +
               " + 1), .length = " + lenStr + " }";
      }
      return "0";
    }

    case NodeType::variableUse: {
      auto& ref = static_cast<VariableNode&>(*node);
      return ref.mangledName.empty() ? ref.name : ref.mangledName;
    }

    case NodeType::collectionUse: {
      return static_cast<CollectionUseNode&>(*node).mangledName;
    }

    case NodeType::binnaryOp: {
      auto& op = static_cast<BinnaryOpNode&>(*node);
      std::string L = transpileExpr(op.left.get(), ctx);
      std::string R = transpileExpr(op.right.get(), ctx);
      auto it = OperatorMap.find(op.op);
      std::string opStr =
          (it != OperatorMap.end()) ? it->second : " /* unknown op */ ";

      return "(" + L + " " + opStr + " " + R + ")";
    }

    case NodeType::functionCall: {
      auto& func = static_cast<FunctionCallNode&>(*node);
      std::string name =
          func.mangledName.empty() ? func.functionName : func.mangledName;
      std::string args;
      for (size_t i = 0; i < func.Args.size(); ++i) {
        args += transpileExpr(func.Args[i].get(), ctx);
        if (i + 1 < func.Args.size()) args += ", ";
      }
      return name + "(" + args + ")";
    }

    default:
      return "0";
  }
}
void transpileStatement(ASTNode* node, TranspileContext& ctx) {
  if (!node) return;

  switch (node->getType()) {
    case NodeType::variableDeclr: {
      auto& var = static_cast<VariableDeclrNode&>(*node);
      std::string name = var.mangledName.empty() ? var.name : var.mangledName;

      if (var.expression->evaluatedType == ExprNode::DataType::String) {
        ctx.registerString(name);
      }

      ctx.emitIndent();
      ctx.body << toCType(var.expression->evaluatedType) << " " << name << " = "
               << transpileExpr(var.expression.get(), ctx) << ";\n";
      break;
    }

    case NodeType::variableOverride: {
      auto& var = static_cast<VariableAssignmentNode&>(*node);
      std::string name = var.mangledName;
      std::string newVal = transpileExpr(var.expression.get(), ctx);

      ctx.emitIndent();
      if (var.expression->evaluatedType == ExprNode::DataType::String) {
        ctx.body << "free(" << name << ".data);\n";
        ctx.emitIndent();
      }
      ctx.body << name << " = " << newVal << ";\n";
      break;
    }

    case NodeType::ifStatement: {
      auto& stmt = static_cast<IfStatementNode&>(*node);
      ctx.emitIndent();
      ctx.body << "if (" << transpileExpr(stmt.condition.get(), ctx) << ") {\n";

      ctx.indentLevel++;
      transpileStatement(stmt.thenBlock.get(), ctx);

      for (const auto& strName : ctx.allocatedStrings.back()) {
        ctx.emitIndent();
        ctx.body << "free(" << strName << ".data);\n";
      }
      ctx.leaveBlock();

      ctx.indentLevel--;

      ctx.emitIndent();
      ctx.body << "}";

      if (stmt.orelse) {
        ctx.body << " else {\n";
        ctx.indentLevel++;
        transpileStatement(stmt.orelse.get(), ctx);
        for (const auto& strName : ctx.allocatedStrings.back()) {
          ctx.emitIndent();
          ctx.body << "free(" << strName << ".data);\n";
        }
        ctx.leaveBlock();
        ctx.indentLevel--;
        ctx.emitIndent();
        ctx.body << "}\n";
      } else {
        ctx.body << "\n";
      }
      break;
    }

    case NodeType::forStatement: {
      auto& forNode = static_cast<ForStatementNode&>(*node);
      if (std::holds_alternative<ForStatementNode::Range>(forNode.source)) {
        auto range = std::get<0>(std::move(forNode.source));
        std::string iter = forNode.mangledName;

        ctx.emitIndent();
        ctx.body << "for (int32_t " << iter << " = "
                 << transpileExpr(range.from.get(), ctx) << "; " << iter
                 << " < " << transpileExpr(range.to.get(), ctx) << "; " << iter
                 << " += " << transpileExpr(range.step.get(), ctx) << ") {\n";

        ctx.indentLevel++;
        for (auto& bodyNode : forNode.body->block) {
          if (bodyNode->getType() == NodeType::break_) {
            for (const auto& strName : ctx.allocatedStrings.back()) {
              ctx.emitIndent();
              ctx.body << "free(" << strName << ".data);\n";
            }
            ctx.emitIndent();
            ctx.body << "break;\n";
          } else {
            transpileStatement(bodyNode.get(), ctx);
          }
        }
        for (const auto& strName : ctx.allocatedStrings.back()) {
          ctx.emitIndent();
          ctx.body << "free(" << strName << ".data);\n";
        }
        ctx.leaveBlock();
        ctx.indentLevel--;
        ctx.emitIndent();
        ctx.body << "}\n";
      }
      break;
    }

    case NodeType::whileStatement: {
      auto& whileNode = static_cast<WhileStatementNode&>(*node);
      ctx.emitIndent();
      ctx.body << "while (" << transpileExpr(whileNode.condition.get(), ctx)
               << ") {\n";

      ctx.indentLevel++;
      for (auto& stmtNode : whileNode.body->block) {
        if (stmtNode->getType() == NodeType::break_) {
          ctx.emitIndent();
          ctx.body << "break;\n";
        } else {
          transpileStatement(stmtNode.get(), ctx);
        }
      }
      ctx.indentLevel--;
      ctx.emitIndent();
      ctx.body << "}\n";
      break;
    }

    case NodeType::functionCall: {
      ctx.emitIndent();
      ctx.body << transpileExpr(node, ctx) << ";\n";
      break;
    }

    case NodeType::returns: {
      auto& ret = static_cast<ReturnNode&>(*node);
      ctx.emitIndent();
      if (ret.returns) {
        ctx.body << "return " << transpileExpr(ret.returns.get(), ctx) << ";\n";
      } else {
        ctx.body << "return;\n";
      }
      break;
    }
    default:
      break;
  }
}

void compile(NodeArray& nrr, [[maybe_unused]] int targetPlatform,
             [[maybe_unused]] int flags) {
  TranspileContext ctx;
  ctx.headers << "#include <stdint.h>\n"
              << "#include <stdbool.h>\n"
              << "#include <stdlib.h>\n"
              << "#include <string.h>\n\n";
  ctx.structs << "typedef struct {\n"
              << "    char* data;\n"
              << "    int64_t length;\n"
              << "} StringView;\n\n";

  for (const auto& node : nrr) {
    if (node->getType() == NodeType::functionDef) {
      auto& fun = static_cast<FunctionDeclNode&>(*node);
      std::string params;
      for (size_t i = 0; i < fun.Params.size(); ++i) {
        if (fun.Params[i].name.empty() && fun.Params[i].mangledName.empty())
          continue;
        std::string pName = fun.Params[i].mangledName.empty()
                                ? (fun.mangledName + "_" + fun.Params[i].name)
                                : fun.Params[i].mangledName;
        params += toCType(fun.Params[i].type) + " " + pName;
        if (i + 1 < fun.Params.size()) params += ", ";
      }
      if (params.empty()) params = "void";
      ctx.prototypes << toCType(fun.evaluatedType) << " " << fun.mangledName
                     << "(" << params << ");\n";
      if (fun.isExtern || !fun.does) continue;
      ctx.body << toCType(fun.evaluatedType) << " " << fun.mangledName << "("
               << params << ") {\n";
      ctx.indentLevel++;
      if (fun.does && fun.does->getType() == NodeType::block) {
        for (const auto& insideNode : static_cast<Block&>(*fun.does).block) {
          transpileStatement(insideNode.get(), ctx);
        }
      }
      ctx.indentLevel--;
      ctx.body << "}\n\n";
    } else if (node->getType() == NodeType::main) {
      auto& mainNode = static_cast<MainNode&>(*node);
      ctx.body << "int main(void) {\n";
      ctx.indentLevel++;
      for (auto& inside : mainNode.logic) {
        transpileStatement(inside.get(), ctx);
      }
      ctx.emitIndent();
      ctx.body << "return 0;\n";
      ctx.indentLevel--;
      ctx.body << "}\n\n";
    }
  }
  std::cout << ctx.headers.str() << ctx.structs.str() << "/* Prototypes /\n"
            << ctx.prototypes.str() << "\n"
            << "/ Implementations */\n"
            << ctx.body.str();
}
