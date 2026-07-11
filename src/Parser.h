#pragma once
#include <memory>
#include <string>
#include <variant>
#include <vector>

#include "Lexer.h"
#include "Token.h"

enum class Operator {
  pow,
  multiply,
  divide,
  plus,
  minus,
  lessThan,
  greaterThan,
  equal,
  notEqual,
  lessOrEqual,
  greaterOrEqual,
  none
};
enum class NodeType {
  variableDeclr,
  variableUse,
  variableOverride,
  collection,
  collectionUse,
  collectionOverride,
  functionCall,
  functionDef,
  main,
  break_,
  ifStatement,
  forStatement,
  whileStatement,
  binnaryOp,
  block,
  classn,
  methodCall,
  constant,
  returns,
  blank
};

class ASTNode {
 public:
  virtual ~ASTNode() = default;
  virtual NodeType getType() const = 0;
  virtual std::string printInfo() { return "unknown"; };
  pos loc = {1, 1};
};

class ExprNode : public ASTNode {
 public:
  enum class DataType {
    String,
    Int,
    Float,
    Float32,
    Bool,
    Pointer,
    Array,
    Class,
    Unknown
  } evaluatedType = ExprNode::DataType::Unknown;
  struct ClassField;
  struct TypeMetadata {
    DataType arrayElementBaseType = DataType::Unknown;
    std::shared_ptr<TypeMetadata> arrayElementMeta = nullptr;

    std::string className;
    struct ClassField {
      std::string name;
      DataType baseType;
      std::shared_ptr<TypeMetadata> meta;
    };
    std::vector<ClassField> fields;
  };

  std::shared_ptr<TypeMetadata> typeMeta = nullptr;
};

class CollectionNode : public ExprNode {
 public:
  std::vector<std::unique_ptr<ExprNode>> objects;
  enum class CollectionType { Array, List } type;
  NodeType getType() const override { return NodeType::collection; }
};

class CollectionUseNode : public ExprNode {
 public:
  std::string name;
  std::string mangledName;
  std::unique_ptr<ExprNode> index;
  NodeType getType() const override { return NodeType::collectionUse; }
};

class CollectionOverrideNode : public ASTNode {
 public:
  std::string name;
  std::string mangledName;
  std::unique_ptr<ExprNode> index;
  std::unique_ptr<ExprNode> content;
  NodeType getType() const override { return NodeType::collectionOverride; }
};

class VariableDeclrNode : public ASTNode {
 public:
  std::string name;
  std::string mangledName;
  std::unique_ptr<ExprNode> expression;
  bool isConstant = false;
  std::shared_ptr<ExprNode::DataType> preparedType = nullptr;
  std::shared_ptr<ExprNode::TypeMetadata> preparedMeta = nullptr;
  std::string printInfo() override {
    return "Variable: name: " + name +
           ", expression: " + expression->printInfo() + ".";
  }
  NodeType getType() const override { return NodeType::variableDeclr; }
};

class VariableAssignmentNode : public ASTNode {
 public:
  std::string name;
  std::string mangledName;
  std::unique_ptr<ExprNode> expression;
  NodeType getType() const override { return NodeType::variableOverride; }
};

class VariableNode : public ExprNode {
 public:
  std::string name;
  std::string mangledName;
  std::string printInfo() override { return "Variable: name: " + name + "."; }
  NodeType getType() const override { return NodeType::variableUse; }
};

class FunctionCallNode : public ExprNode {
 public:
  std::vector<std::unique_ptr<ExprNode>> Args;
  std::string functionName;
  std::string mangledName;
  std::string printInfo() override {
    return "Function: name: " + functionName + ", args: " + [this]() {
      std::string res;
      for (const auto& expr : Args) {
        res += expr->printInfo() + " ";
      }
      return res;
    }();
  }
  NodeType getType() const override { return NodeType::functionCall; }
};

struct Arg {
  std::string name;
  std::string mangledName = name;
  ExprNode::DataType type = ExprNode::DataType::Unknown;
  std::shared_ptr<ExprNode::TypeMetadata> typeMeta = nullptr;
};

class FunctionDeclNode : public ExprNode {
 public:
  std::vector<Arg> Params;
  std::string functionName;
  std::string mangledName;
  std::unique_ptr<ExprNode> does;
  bool isExtern = false;
  NodeType getType() const override { return NodeType::functionDef; }
};

class Break : public ASTNode {
 public:
  NodeType getType() const override { return NodeType::break_; }
};

class Continue : public ASTNode {
	NodeType getType() const override {
		return NodeType::continue_;
	}
};

class Namespace : public ASTNode {
	std::string np;
	std::vector<std::unique_ptr<ASTNode>> nrr;
	NodeType getType() const override {
		return NodeType::namespace_;
	}
};

class LiteralNode : public ExprNode {
 public:
  std::string value;
  std::string printInfo() { return value; }
  NodeType getType() const override { return NodeType::constant; }
};

class ReturnNode : public ExprNode {
 public:
  std::unique_ptr<ExprNode> returns;
  NodeType getType() const override { return NodeType::returns; }
};

class BinnaryOpNode : public ExprNode {
 public:
  Operator op = Operator::none;
  std::unique_ptr<ExprNode> left;
  std::unique_ptr<ExprNode> right;

  std::string printInfo() override {
    return (left == nullptr ? "" : left->printInfo()) + "<-->" +
           (right == nullptr ? "" : right->printInfo());
  }
  NodeType getType() const override { return NodeType::binnaryOp; }
};

class Block : public ExprNode {
 public:
  std::vector<std::unique_ptr<ASTNode>> block;
  NodeType getType() const override { return NodeType::block; }
};

class IfStatementNode : public ASTNode {
 public:
  std::unique_ptr<ExprNode> condition;
  std::unique_ptr<Block> thenBlock;
  std::unique_ptr<ASTNode> orelse;
  NodeType getType() const override { return NodeType::ifStatement; }
};

class ForStatementNode : public ASTNode {
 public:
  std::string varName;
  std::string mangledName;
  struct Range {
    std::unique_ptr<ExprNode> from;
    std::unique_ptr<ExprNode> to;
    std::unique_ptr<ExprNode> step;
  };
  struct Collection {
    std::unique_ptr<ExprNode> collection;
  };
  std::variant<Range, Collection> source;
  std::unique_ptr<Block> body;
  NodeType getType() const override { return NodeType::forStatement; }
};

class WhileStatementNode : public ASTNode {
 public:
  std::unique_ptr<ExprNode> condition;
  std::unique_ptr<Block> body;
  NodeType getType() const override { return NodeType::whileStatement; }
};

class ClassNode : public ExprNode {
 public:
  std::string name;
  std::string mangledName;
  std::vector<std::unique_ptr<ASTNode>> _public;
  std::vector<std::unique_ptr<ASTNode>> _private;
  ClassNode* extends = nullptr;
  NodeType getType() const override { return NodeType::classn; }
};

class MethodCallNode : public FunctionCallNode {
 public:
  std::unique_ptr<ExprNode> object;
  const ClassNode* classDefinition = nullptr;

  bool isStatic = false;
  bool isBuiltin = false;
  NodeType getType() const override { return NodeType::methodCall; }
};

class MainNode : public ExprNode {
 public:
  std::vector<std::unique_ptr<ASTNode>> logic;
  NodeType getType() const override { return NodeType::main; }
};

using NodeArray = std::vector<std::unique_ptr<ASTNode>>;

NodeArray Parse(TokenArray arr);
