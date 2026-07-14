#include "Parser.h"

#include <stdio.h>
#include <unordered_map>

#include "diagnostics.h"
#include "parse_helper.h"

#include "HandleBring.h"

static Operator getOperator(TokenType type) {
  switch (type) {
    case TokenType::plus:
      return Operator::plus;
    case TokenType::minus:
      return Operator::minus;
    case TokenType::multiply:
      return Operator::multiply;
    case TokenType::divide:
      return Operator::divide;
    case TokenType::gt:
      return Operator::greaterThan;
    case TokenType::lt:
      return Operator::lessThan;
    case TokenType::equequ:
      return Operator::equal;
    case TokenType::not_equ:
      return Operator::notEqual;
    default:
      return Operator::plus;
  }
}

inline TokenType chTt(size_t i, const TokenArray& arr) {
  if (i > arr.size()) return TokenType::EoF;
  return arr[i].type;
}

bool isEOF(Token t) { return t.type == TokenType::EoF; }

std::unique_ptr<ExprNode> ParseExpression(size_t& i, const TokenArray& arr);
std::unique_ptr<ExprNode> ParseSum(size_t& i, const TokenArray& arr);
std::unique_ptr<ASTNode> ParseBasic(size_t& i, const TokenArray& arr);
std::unique_ptr<FunctionCallNode> ParseFunction(size_t& i, const TokenArray& arr);
std::unique_ptr<Block> ParseBlock(size_t& i, const TokenArray& arr);

bool fq = false;
std::unordered_map<std::string, bool> brought;

struct ParsedTypeResult {
  ExprNode::DataType baseType = ExprNode::DataType::Unknown;
  std::shared_ptr<ExprNode::TypeMetadata> meta = nullptr;
};

ParsedTypeResult ParseType(size_t& i, const TokenArray& arr) {
  ParsedTypeResult result{ExprNode::DataType::Unknown};
  if (i >= arr.size() || arr[i].type != TokenType::identifier) return result;
  ExprNode::DataType& type = result.baseType;
  if (arr[i].content == "int") {
    type = ExprNode::DataType::Int;
    i++;
  } else if (arr[i].content == "float") {
    type = ExprNode::DataType::Float;
    i++;
  } else if (arr[i].content == "float32") {
    type = ExprNode::DataType::Float32;
    i++;
  } else if (arr[i].content == "bool") {
    type = ExprNode::DataType::Bool;
    i++;
  } else if (arr[i].content == "string") {
    type = ExprNode::DataType::String;
    i++;
  } else if (arr[i].content == "ptr") {
    type = ExprNode::DataType::Pointer;
    i++;
    if (arr[i].type == TokenType::lt) {
      i++;
      ParsedTypeResult inner = ParseType(i, arr);
      result.meta = std::make_shared<ExprNode::TypeMetadata>();
      result.meta->arrayElementBaseType = inner.baseType;
      result.meta->arrayElementMeta = inner.meta;
      if (arr[i].type != TokenType::gt) {
        errorf(arr[i].loc, "P114", "Expected '>'");
        return result;
      }
      i++;
    }
  } else if (arr[i].content == "items") {
    type = ExprNode::DataType::Array;
    i++;
    if (arr[i].type == TokenType::lt) {
      i++;
      ParsedTypeResult inner = ParseType(i, arr);
      result.meta = std::make_shared<ExprNode::TypeMetadata>();
      result.meta->arrayElementBaseType = inner.baseType;
      result.meta->arrayElementMeta = inner.meta;
      if (arr[i].type != TokenType::gt) {
        errorf(arr[i].loc, "P114", "Expected '>'");
        return result;
      }
      i++;
    }
  } else {
    type = ExprNode::DataType::Class;
    result.meta = std::make_shared<ExprNode::TypeMetadata>();
    result.meta->className = arr[i].content;
    i++;
  }
  return result;
}

std::unique_ptr<Namespace> ParseBring(size_t& i, const TokenArray& arr) {
	if (arr[i].type != TokenType::bring) {
		return nullptr;
	}
	auto namespace_ = std::make_unique<Namespace>();
	i++; // keyword
	if (arr[i].type != TokenType::identifier || arr[i].type != TokenType::string) {
		errorf(arr[i].loc, "P210|P157", "Expected an identifier or string after \"bring\"");
		i++;
		return nullptr;
	}
	std::string importName;
	namespace_->nrr = HandleBring(arr[i].content);
	i++;
	if (arr[i].type != TokenType::arrow) {
		size_t lastDotPosition = importName.rfind(".");
		namespace_->np = (lastDotPosition != std::string::npos)
			? importName.substr(lastDotPosition)
			: importName;
		return namespace_;
	}
	i++; // ->
	if (arr[i].type != TokenType::identifier) {
		errorf(arr[i].loc, "P210", "Expetced and indentifier after '->'");
		return nullptr;
	}
	namespace_->np = arr[i].content;
	return namespace_;
}

std::unique_ptr<Namespace> ParseNamespace(size_t& i, const TokenArray& arr) {
	if (arr[i].type != TokenType::namespace_) {
		return nullptr;
	}
	auto namespace_ = std::make_unique<Namespace>();
	i++; //keyword
	if (arr[i].type != TokenType::identifier) {
		errorf(arr[i].loc, "P210", "Expected namespace name, after keyword");
		return nullptr;
	}
	namespace_->np = arr[i].content;
	i++; // name
	if (arr[i].type != TokenType::lbrace) {
		errorf(arr[i].loc, "P153", "Expected '{{' after {}", namespace_->np);
		return nullptr;
	}
	namespace_->nrr = std::move(ParseBlock(i, arr)->block);
	return namespace_;
}

std::unique_ptr<MethodCallNode> ParseDotCall(size_t& i, const TokenArray& arr, std::unique_ptr<ExprNode> Parent) {
	if (arr[i].type != TokenType::dot) {
		return nullptr;
	}
	i++; // .
	auto methodCall = std::make_unique<MethodCallNode>();
	auto function = ParseFunction(i, arr);
	static_cast<FunctionCallNode&>(*methodCall) = std::move(*function);
	methodCall->object = std::move(Parent);
	if (arr[i].type == TokenType::dot) {
		return ParseDotCall(i, arr, std::move(methodCall));
	}
	return methodCall;
}

std::unique_ptr<MethodCallNode> ParseNamedMethodCall(size_t& i, const TokenArray& arr) {
	if (arr[i].type != TokenType::identifier && arr[i+1].type != TokenType::dot) {
		return nullptr;
	}
	// Clossest we can get
	auto parent = std::make_unique<VariableNode>();
	parent->name = arr[i].content;
	i++;
	auto methodCall = ParseDotCall(i, arr, std::move(parent));
	return methodCall;
}

std::unique_ptr<ReturnNode> ParseReturn(size_t& i, const TokenArray& arr) {
  if (arr[i].type == TokenType::return_) {
    auto _return = std::make_unique<ReturnNode>();
    i++;
    _return->loc = arr[i].loc;
    _return->returns = ParseExpression(i, arr);
    return _return;
  }
  return nullptr;
}

std::unique_ptr<FunctionCallNode> ParseFunction(size_t& i,
                                                const TokenArray& arr) {
  if (arr[i].type == TokenType::identifier &&
      chTt(i + 1, arr) == TokenType::lparen) {
    auto node = std::make_unique<FunctionCallNode>();
    node->loc = arr[i].loc;
    node->functionName = arr[i].content;
    i += 2;
    // std::vector<std::unique_ptr<ExprNode>> args;
    while (i < arr.size() && arr[i].type != TokenType::rparen) {
      if (arr[i].type == TokenType::comma) {
        // Может не просто игнорть?
        i++;
        if (arr[i].type == TokenType::rparen) {
          errorf(node->loc, "P152", "Trailing comma in function call.");
        }
      }
      node->Args.push_back(ParseExpression(i, arr));
    }
    if (i < arr.size() && arr[i].type == TokenType::rparen) {
      i++;
    }
    return node;
  }
  return nullptr;
}

std::unique_ptr<VariableDeclrNode> ParseVariableDeclr(size_t& i,
                                                      const TokenArray& arr) {
  if (arr[i].type == TokenType::let || arr[i].type == TokenType::const_) {
    std::unique_ptr<VariableDeclrNode> tempNode =
        std::make_unique<VariableDeclrNode>();
    tempNode->loc = arr[i].loc;
    tempNode->isConstant = arr[i].type == TokenType::const_;
    i++;  // let / const
    if (arr[i].type != TokenType::identifier) {
      errorf(tempNode->loc, "P210", "Missing variable name.");

      return nullptr;
      i++;
    }
    tempNode->name = arr[i].content;
    i++;  // name
    if (isEOF(arr[i])) {
      errorf(tempNode->loc, "P154", "expected '=' after \"{}\", got \"{}{}\".",
             tempNode->name, getByTokenType(arr[i].type), arr[i].content);
      return nullptr;
      i++;
    }
    i++;
    if (isEOF(arr[i])) {
      errorf(tempNode->loc, "P154", "expected value after '=' for {}",
             tempNode->name);
      return nullptr;
    }
    tempNode->expression = ParseExpression(i, arr);
    return tempNode;
  }
  return nullptr;
}

std::unique_ptr<ASTNode> ParseCollectionIdnt(size_t& i, const TokenArray& arr) {
  std::string name = arr[i].content;
  i++;
  if (arr[i].type != TokenType::lbracket) {
    return nullptr;
  }
  i++;  // [
  auto index = ParseExpression(i, arr);
  if (arr[i].type != TokenType::rbracket) {
    errorf(arr[i].loc, "P112", "Expected ']' after expression in {}", name);
    return nullptr;
  }
  i++;
  if (arr[i].type == TokenType::equ) {
    auto node = std::make_unique<CollectionOverrideNode>();
    node->name = name;
    node->index = std::move(index);
    node->content = ParseExpression(i, arr);
    return node;
  } else {
    auto node = std::make_unique<CollectionUseNode>();
    node->name = name;
    node->index = std::move(index);
    return node;
  }
}

std::unique_ptr<VariableAssignmentNode> ParseVariableAss(
    size_t& i, const TokenArray& arr) {
  if (arr[i].type == TokenType::identifier &&
      arr[i + 1].type == TokenType::equ) {
    auto node = std::make_unique<VariableAssignmentNode>();
    node->loc = arr[i].loc;
    node->name = arr[i].content;
    i += 2;
    if (isEOF(arr[i])) {
      errorf(node->loc, "P154", "expected value after '=' for {}", node->name);
      return nullptr;
    }
    node->expression = ParseExpression(i, arr);
    if (!node->expression) {
      errorf(arr[i].loc, "P999", "Parse failed on a reason");
      return nullptr;
    }
    return node;
  }
  return nullptr;
}

// Колекция может быть чем угодно, и из этого у меня появилась трудность
// Например
// collection users = ["Anton", "Jon", "Michael", "Peter"]
// А может
// ["Anton", "Jon", "Michael", "Peter"]
// или
// users
// хотя можно например сделать обозначение
// multiple users
// Но, это дополнительный кейворд, и новая морока
std::unique_ptr<CollectionNode> ParseCollection(size_t& i,
                                                const TokenArray& arr) {
  if (i >= arr.size() || arr[i].type != TokenType::lbracket) {
    return nullptr;
  }

  auto node = std::make_unique<CollectionNode>();
  node->loc = arr[i].loc;
  i++; // [

  bool expect_element = true;

  while (i < arr.size() && arr[i].type != TokenType::rbracket) {
    if (arr[i].type == TokenType::comma) {
      if (expect_element) {
        errorf(arr[i].loc, "P151", "Unexpected comma.");
        return nullptr;
      }
      i++;  // ,

      if (i < arr.size() && arr[i].type == TokenType::rbracket) {
        errorf(arr[i].loc, "P152", "Trailing comma in collection.");
      }
      expect_element = true;
      continue;
    }

    if (!expect_element) {
      errorf(arr[i].loc, "P155", "Expected ',' between collection elements.");
      return nullptr;
    }

    auto expr = ParseExpression(i, arr);
    if (!expr) return nullptr;

    node->objects.push_back(std::move(expr));
    expect_element = false;
  }

  if (i >= arr.size() || arr[i].type != TokenType::rbracket) {
    errorf(i < arr.size() ? arr[i].loc : arr.back().loc, "P112",
           "Expected ']' to end expression");
    return nullptr;
  }

  i++;
  return node;
}

std::unique_ptr<Break> ParseBreak(size_t& i, const TokenArray& arr) {
  if (arr[i].type == TokenType::break_) {
    auto break_ = std::make_unique<Break>();  // брык
    break_->loc = arr[i].loc;                 // брик
    i++;
    return break_;  // брэк
  }
  return nullptr;
}

std::unique_ptr<Continue> ParseContinue(size_t& i, const TokenArray& arr) {
	if (arr[i].type == TokenType::continue_) {
		auto continue_ = std::make_unique<Continue>();
		continue_->loc = arr[i].loc;
		i++;
		return continue_;
	}
	return nullptr;
}

std::unique_ptr<Block> ParseBlock(size_t& i, const TokenArray& arr) {
  if (arr[i].type == TokenType::lbrace) {
    std::unique_ptr<Block> block = std::make_unique<Block>();
    block->loc = arr[i].loc;
    i++;
    while (i < arr.size() && arr[i].type != TokenType::rbrace) {
      if (arr[i].type == TokenType::return_) {
        block->block.push_back(ParseReturn(i, arr));
      } else {
        block->block.push_back(ParseBasic(i, arr));
      }
    }
    i++;
    return block;
  }
  return nullptr;
}

std::unique_ptr<FunctionDeclNode> ParseFunctionDeclr(size_t& i,
                                                     const TokenArray& arr) {
  if (arr[i].type == TokenType::function) {
    auto functionDeclr = std::make_unique<FunctionDeclNode>();
    functionDeclr->loc = arr[i].loc;
    i++;
    functionDeclr->functionName = arr[i].content;
    i++;
    if (i >= arr.size() || arr[i].type != TokenType::lparen) {
      errorf(functionDeclr->loc, "P153", "Expected '(' after function name.");
      return nullptr;
    }
    i++;  // '('
    while (i < arr.size() && arr[i].type != TokenType::rparen) {
      if (arr[i].type == TokenType::comma) {
        i++;
        if (arr[i].type == TokenType::rparen) {
          errorf(functionDeclr->loc, "P152",
                 "Trailing comma in function call.");
        }
        continue;
      }
      Arg arg;
      if (arr[i].type != TokenType::identifier) {
        //
        if (arr[i].type == TokenType::rparen) {
          break;
        }
        errorf(arr[i].loc, "P155", "Expected argument name.");
        return nullptr;
      }
      arg.name = arr[i].content;
      i++;  // Имя
      ParsedTypeResult parsedType = ParseType(i, arr);
      if (parsedType.baseType == ExprNode::DataType::Unknown) {
        errorf(arr[i - 1].loc, "P156",
               "Expected valid type after argument name.");
        return nullptr;
      }
      arg.type = parsedType.baseType;
      arg.typeMeta = parsedType.meta;

      functionDeclr->Params.push_back(arg);
    }
    if (i < arr.size() && arr[i].type == TokenType::rparen) {
      i++;
    }
    if (i >= arr.size()) {
      errorf(functionDeclr->loc, "P201", "Unexpected end of file.");
      return nullptr;
    }
    if (arr[i].type == TokenType::identifier &&
        chTt(i + 1, arr) == TokenType::lparen) {
      functionDeclr->does = ParseFunction(i, arr);
    } else if (arr[i].type == TokenType::lbrace) {
      functionDeclr->does = ParseBlock(i, arr);
    } else if (arr[i].type == TokenType::return_) {
      functionDeclr->does = ParseReturn(i, arr);
    } else {
      errorf(functionDeclr->loc, "P201", "Missing implementation of \"{}\".",
             functionDeclr->functionName);
      i++;
      return nullptr;
    }
    std::cout << "Function added: " << functionDeclr->functionName << "\n";
    return functionDeclr;
  }
  return nullptr;
}
std::unique_ptr<FunctionDeclNode> ParseExternFunc(size_t& i,
                                                  const TokenArray& arr) {
  if (arr[i].type != TokenType::extern_ ||
      arr[i + 1].type != TokenType::function) {
    return nullptr;
  }
  i += 2;
  auto node = std::make_unique<FunctionDeclNode>();
  node->isExtern = true;
  node->functionName = arr[i].content;
  i++;
  if (arr[i].type != TokenType::lparen) {
    errorf(node->loc, "P101", "expected '(' after {}", node->functionName);
    return nullptr;
  }
  i++;
  while (i < arr.size() && arr[i].type != TokenType::rparen) {
    if (arr[i].type == TokenType::comma) {
      i++;
      if (arr[i].type == TokenType::rparen) {
        errorf(node->loc, "P152", "Trailing comma in function call.");
      }
    }
    Arg arg;
    arg.name = arr[i].content;
    i++;  // Имя
    if (i >= arr.size() || (arr[i].type != TokenType::identifier &&
                            arr[i].type != TokenType::lbracket)) {
      errorf(arr[i - 1].loc, "P156", "Expected type after argument name.");
      return nullptr;
    }
    ExprNode::DataType dt;
    if (arr[i].type == TokenType::lbracket) {
      i++;  // '['
      arg.type = ExprNode::DataType::Array;
      if (i >= arr.size()) {
        errorf(arr[i - 1].loc, "P156", "Expected type inside brackets.");
        return nullptr;
      }
    }
    if (arr[i].content == "int") {
      dt = ExprNode::DataType::Int;
      i++;
    } else if (arr[i].content == "float") {
      dt = ExprNode::DataType::Float;
      i++;
    } else if (arr[i].content == "bool") {
      dt = ExprNode::DataType::Bool;
      i++;
    } else if (arr[i].content == "string") {
      dt = ExprNode::DataType::String;
      i++;
    } else
      arg.type = ExprNode::DataType::Class;
    if (arg.type != ExprNode::DataType::Array)
      arg.type = dt;
    else if (arg.type == ExprNode::DataType::Class)
      arg.typeMeta->className = arr[i].content;
    else {
      arg.typeMeta->arrayElementBaseType = dt;
      if (arr[i].type != TokenType::rbracket) {
        errorf(arr[i].loc, "P112", "Expected ']' to end expression");
        return nullptr;
      }
      i++;
    }
    node->Params.push_back(arg);
  }
  if (i < arr.size() && arr[i].type == TokenType::rparen) {
    i++;
  }
  if (arr[i].type != TokenType::arrow) {
    errorf(arr[i].loc, "P120",
           "Expected \"->\" after function parameters of {}. extern functions "
           "have structure like: \"extern function add(a int, b int) -> int\"",
           node->functionName);
    return nullptr;
  }
  i++;
  ParsedTypeResult type = ParseType(i, arr);

  node->evaluatedType = type.baseType;
  node->typeMeta = type.meta;
  return node;
}

std::unique_ptr<ASTNode> ParseStatement(size_t& i, const TokenArray& arr) {
  if (arr[i].type == TokenType::if_ || arr[i].type == TokenType::for_ ||
      arr[i].type == TokenType::while_) {
    if (arr[i].type == TokenType::if_) {
      std::unique_ptr<IfStatementNode> node =
          std::make_unique<IfStatementNode>();
      node->loc = arr[i].loc;
      i++;
      if (arr[i].type != TokenType::lparen) {
        errorf(node->loc, "P101", "expected '(' after \"if\"");
        return nullptr;
      }
      i++;
      node->condition = ParseExpression(i, arr);
      if (!node->condition) return nullptr;
      if (arr[i].type != TokenType::rparen) {
        errorf(node->loc, "P102",
               "expected closing pathenses ')' after condition");
        return nullptr;
      }
      i++;
      node->thenBlock = ParseBlock(i, arr);
      if (arr[i].type == TokenType::else_) {
        i++;
        if (arr[i].type == TokenType::if_) {
          node->orelse = ParseStatement(i, arr);
        } else if (arr[i].type == TokenType::lbrace) {
          node->orelse = ParseBlock(i, arr);
        }
      }
      return node;
    }
    if (arr[i].type == TokenType::while_) {
      auto node = std::make_unique<WhileStatementNode>();
      node->loc = arr[i].loc;
      i++;
      if (arr[i].type != TokenType::lparen) {
        errorf(node->loc, "P101", "expected '(' after \"while\"");
        return nullptr;
      }
      i++;
      node->condition = ParseExpression(i, arr);
      if (arr[i].type != TokenType::rparen) {
        errorf(node->loc, "P102", "expected ')' after condition, got {}",
               static_cast<int>(arr[i].type));
        return nullptr;
      }
      i++;
      node->body = ParseBlock(i, arr);
      return node;
    }
    if (arr[i].type == TokenType::for_) {
      auto node = std::make_unique<ForStatementNode>();
      i++;
      if (arr[i].type != TokenType::lparen) {
        // Какая-то ошибка
        errorf(node->loc, "P101", "expected '(' after \"for\"");
        return nullptr;
      }
      i++;
      // А это уже (теперь не) сложно
      // SynTAX
      // for ( index -> start:end ) { code }
      // for ( index -> start:end -> step) { code }
      // for ( item:list ) { code }
      node->varName = arr[i].content;
      if ((arr[i + 1].type == TokenType::arrow) ||
          (arr[i + 1].type == TokenType::colon)) {
        if (arr[i + 1].type == TokenType::arrow) {
          ForStatementNode::Range range;
          i += 2;
          range.from = ParseSum(i, arr);
          i++;  // :
          range.to = ParseSum(i, arr);
          if (arr[i].type == TokenType::arrow) {
            i++;
            range.step = ParseSum(i, arr);
          } else {
            auto one = std::make_unique<LiteralNode>();
            one->evaluatedType = ExprNode::DataType::Int;
            one->loc = node->loc;
            one->value = "1";
            range.step = std::move(one);
          }
          node->source = std::move(range);

        } else if (arr[i + 1].type == TokenType::colon) {
          ForStatementNode::Collection collection;
          i += 2;
          // Не готово, потому что нет ещё парсера коллекций
          node->source = std::move(collection);
        }
        // i++;
        if (arr[i].type != TokenType::rparen) {
          errorf(node->loc, "P101", "expected ')' after expression in \"for\"");
          return nullptr;
        }
        i++;
        node->body = ParseBlock(i, arr);
        return node;
      }
      return nullptr;
    }
  }
  return nullptr;
}

std::unique_ptr<LiteralNode> ParseLiteral(size_t& i, const TokenArray& arr) {
  if (arr[i].type == TokenType::string || arr[i].type == TokenType::number ||
      arr[i].type == TokenType::boolean || arr[i].type == TokenType::float_) {
    TokenType t = arr[i].type;
    auto node = std::make_unique<LiteralNode>();
    node->loc = arr[i].loc;
    if (t == TokenType::number)
      node->evaluatedType = LiteralNode::DataType::Int;
    else if (t == TokenType::float_)
      node->evaluatedType = LiteralNode::DataType::Float;
    else if (t == TokenType::boolean)
      node->evaluatedType = LiteralNode::DataType::Bool;
    else if (t == TokenType::string)
      node->evaluatedType = LiteralNode::DataType::String;

    node->value = arr[i].content;
    i++;
    return node;
  }
  return nullptr;
}

std::unique_ptr<ExprNode> ParseParentheses(size_t& i, const TokenArray& arr) {
  if (arr[i].type == TokenType::lparen) {
    i++;
    auto node = ParseExpression(i, arr);
    node->loc = arr[i].loc;
    if (arr[i].type != TokenType::rparen) {
      errorf(node->loc, "P102", "expected closing parenthesis");
      return nullptr;
    }
    i++;
    return node;
  }
  return nullptr;
}

std::unique_ptr<ExprNode> ParsePrimary(size_t& i, const TokenArray& arr) {
  if (arr[i].type == TokenType::identifier &&
      chTt(i + 1, arr) == TokenType::lparen) {
    return ParseFunction(i, arr);
  }
  if (arr[i].type == TokenType::string || arr[i].type == TokenType::number ||
      arr[i].type == TokenType::boolean || arr[i].type == TokenType::float_) {
    return ParseLiteral(i, arr);
  }
  if (arr[i].type == TokenType::identifier) {
    auto var = std::make_unique<VariableNode>();
    var->loc = arr[i].loc;
    var->name = arr[i].content;
    i++;
    return var;
  }
  if (arr[i].type == TokenType::lparen) {
    return ParseParentheses(i, arr);
  }
  if (arr[i].type == TokenType::lbracket) {
    return ParseCollection(i, arr);
  }
  if (!isEOF(arr[i]))
    errorf(arr[i].loc, "P153", "Unexpected token type, got {}{}.",
           getByTokenType(arr[i].type), arr[i].content);
  return nullptr;
}

std::unique_ptr<ExprNode> ParseTerm(size_t& i, const TokenArray& arr) {
  auto left = ParsePrimary(i, arr);
  while (arr[i].type == TokenType::multiply ||
         arr[i].type == TokenType::divide) {
    auto expr = std::make_unique<BinnaryOpNode>();
    expr->loc = arr[i].loc;
    expr->left = std::move(left);
    expr->op = getOperator(arr[i].type);
    i++;
    expr->right = ParsePrimary(i, arr);
    left = std::move(expr);
  }
  return left;
}

std::unique_ptr<ExprNode> ParseSum(size_t& i, const TokenArray& arr) {
  auto left = ParseTerm(i, arr);
  while (arr[i].type == TokenType::plus || arr[i].type == TokenType::minus) {
    std::unique_ptr<BinnaryOpNode> expr = std::make_unique<BinnaryOpNode>();
    expr->loc = arr[i].loc;

    expr->left = std::move(left);
    expr->op = getOperator(arr[i].type);
    i++;
    expr->right = ParseTerm(i, arr);
    left = std::move(expr);
  }
  return left;
}

std::unique_ptr<ExprNode> ParseExpression(size_t& i, const TokenArray& arr) {
  auto left = ParseSum(i, arr);
  while (arr[i].type == TokenType::equequ ||
         arr[i].type == TokenType::not_equ || arr[i].type == TokenType::gt ||
         arr[i].type == TokenType::lt) {
    std::unique_ptr<BinnaryOpNode> expr = std::make_unique<BinnaryOpNode>();
    expr->loc = arr[i].loc;

    expr->left = std::move(left);
    expr->op = getOperator(arr[i].type);
    i++;
    expr->right = ParseSum(i, arr);
    left = std::move(expr);
  }
  return left;
}

std::unique_ptr<MainNode> ParseMain(size_t& i, const TokenArray& arr) {
  auto node = std::make_unique<MainNode>();
  i++;
  if (arr[i].type == TokenType::lbrace) {
    std::unique_ptr<Block> block = ParseBlock(i, arr);
    node->logic = std::move(block->block);
    return node;
  }
  while (i < arr.size() && arr[i].type != TokenType::EoF) {
    size_t start_i = i;
    if (arr[i].type == TokenType::return_) {
      node->logic.push_back(ParseReturn(i, arr));
    } else {
      node->logic.push_back(ParseBasic(i, arr));
    }
    if (i == start_i) {
      break;
    }
  }
  fq = true;
  return node;
}

std::unique_ptr<ASTNode> ParseBasic(size_t& i, const TokenArray& arr) {
  if (arr[i].type == TokenType::EoF) return nullptr;
  if (arr[i].type == TokenType::let || arr[i].type == TokenType::const_) {
    auto varDeclrNode = std::make_unique<VariableDeclrNode>();
    varDeclrNode = ParseVariableDeclr(i, arr);
    return varDeclrNode;
  }
  if (arr[i].type == TokenType::identifier &&
      chTt(i + 1, arr) == TokenType::lparen) {
    auto functionNode = std::make_unique<FunctionCallNode>();
    functionNode = ParseFunction(i, arr);
    return functionNode;
  }
  if (arr[i].type == TokenType::if_ || arr[i].type == TokenType::for_ ||
      arr[i].type == TokenType::while_) {
    return ParseStatement(i, arr);
  }
  if (arr[i].type == TokenType::function) {
    return ParseFunctionDeclr(i, arr);
  }
  if (arr[i].type == TokenType::extern_ &&
      arr[i + 1].type == TokenType::function) {
    return ParseExternFunc(i, arr);
  }
  if (arr[i].type == TokenType::identifier &&
      chTt(i + 1, arr) == TokenType::lbracket) {
    return ParseCollectionIdnt(i, arr);
  }
  if (arr[i].type == TokenType::break_) {
    return ParseBreak(i, arr);
  }
  if (arr[i].type == TokenType::continue_) {
		return ParseContinue(i, arr);
	} else {
    errorf(arr[i].loc, "P153", "unknown token: {}{}", arr[i].content,
           getByTokenType(arr[i].type));
    i++;
    return nullptr;
  }
}

NodeArray Parse(TokenArray arr) {
  NodeArray temp;
  size_t i = 0;
  while (i < arr.size()) {
    if (isEOF(arr[i])) break;
    if (arr[i].type == TokenType::entry) {
      temp.push_back(ParseMain(i, arr));
    }
    // 👍
    if (!fq) {
      temp.push_back(ParseBasic(i, arr));
    } else {
      break;
    }
    /* Для истории, как всё начиналось:
    * И мои первые подходы к парсингу
    if (arr[i].type == TokenType::let || arr[i].type == TokenType::const_) {
            VariableDeclrNode tempNode;
            i++;
            if (arr[i].type != TokenType::identifier) {
                    fprintf(stderr, "[Error] Missing variable name.\n");
                    i++;
                    continue;
            }
            tempNode.name = arr[i].content;
            i++;
            if (arr[i].type != TokenType::equ) {
                    fprintf(stderr, "[Error] expected '=' after \"%s\".\n",
    tempNode.name.c_str()); i++; continue;
            }
            // Что же делать тут?
    }
    else {
            fprintf(stderr, "[Error] bad syntax.\n");
            i++;
    }
    */
  }

  return temp;
}
