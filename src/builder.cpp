#include "builder.h"

#include <llvm/IR/Function.h>
#include <llvm/IR/IRBuilder.h>
#include <llvm/IR/Intrinsics.h>
#include <llvm/IR/LLVMContext.h>
#include <llvm/IR/Module.h>
#include <llvm/IR/Verifier.h>
#include <llvm/Support/raw_ostream.h>
#include <llvm/Support/FileSystem.h>

#include <llvm/IR/LegacyPassManager.h>
#include <llvm/MC/TargetRegistry.h>
#include <llvm/Support/FileSystem.h>
#include <llvm/Support/TargetSelect.h>
#include <llvm/Target/TargetMachine.h>
#include <llvm/Target/TargetOptions.h>
#include <llvm/TargetParser/Host.h>
#include <llvm/TargetParser/Triple.h>

#include <llvm/Support/CodeGen.h>

#include <iostream>
#include <map>

#include <optional>

std::map<std::string, llvm::AllocaInst*> NamedValues;

llvm::Type* getTy(ExprNode::DataType type, llvm::LLVMContext& Context) {
  switch (type) {
    using enum ExprNode::DataType;
    case Int:
      return llvm::Type::getInt32Ty(Context);
    case Float:
      return llvm::Type::getDoubleTy(Context);
    case Bool:
      return llvm::Type::getInt1Ty(Context);
    case String: {
      llvm::Type* ptrType = llvm::PointerType::getUnqual(Context);
      llvm::Type* lengthType = llvm::Type::getInt64Ty(Context);
      return llvm::StructType::get(Context, {ptrType, lengthType});
    }
    default:
      return llvm::Type::getVoidTy(Context);
  }
}

llvm::Value* codegenExpr(ASTNode* node, llvm::IRBuilder<>& Builder,
                         llvm::LLVMContext& Context, llvm::Module* M);

llvm::Value* buildFuncCall(ASTNode* node, llvm::IRBuilder<>& Builder,
                           llvm::LLVMContext& Context, llvm::Module* M) {
  if (node->getType() == NodeType::functionCall) {
    auto& func = static_cast<FunctionCallNode&>(*node);

    std::string insertName =
        func.mangledName.empty() ? func.functionName : func.mangledName;
    llvm::Function* calleeFn = M->getFunction(insertName);
    llvm::FunctionType* FuncTy = nullptr;

    if (calleeFn) {
      FuncTy = calleeFn->getFunctionType();
    } else {
      llvm::Type* Ty = getTy(func.evaluatedType, Context);
      if (!Ty) Ty = llvm::Type::getVoidTy(Context);

      std::vector<llvm::Type*> argTy;
      for (auto& arg : func.Args) {
        llvm::Type* t = getTy(arg->evaluatedType, Context);
        if (!t || t->isVoidTy()) t = llvm::Type::getInt32Ty(Context);
        argTy.push_back(t);
      }
      FuncTy = llvm::FunctionType::get(Ty, argTy, false);
    }

    llvm::FunctionCallee Func = M->getOrInsertFunction(insertName, FuncTy);

    std::vector<llvm::Value*> Args;
    for (auto& arg : func.Args) {
      llvm::Value* argVal = codegenExpr(arg.get(), Builder, Context, M);
      if (!argVal) argVal = Builder.getInt32(0);
      Args.push_back(argVal);
    }
    llvm::Value* callInst = Builder.CreateCall(Func, Args);
    return callInst;
  }
  return nullptr;
}

llvm::Value* codegenExpr(ASTNode* node, llvm::IRBuilder<>& Builder,
                         llvm::LLVMContext& Context, llvm::Module* M) {
  if (!node) return nullptr;

  if (node->getType() == NodeType::constant) {
    auto& lit = static_cast<LiteralNode&>(*node);
    if (lit.evaluatedType == ExprNode::DataType::Int) {
      int value = std::stoi(lit.value);
      return Builder.getInt32(value);
    } else if (lit.evaluatedType == ExprNode::DataType::Float) {
      float value = std::stod(lit.value);
      return llvm::ConstantFP::get(Context, llvm::APFloat(value));
    } else if (lit.evaluatedType == ExprNode::DataType::Bool) {
      bool value = lit.value == "true";
      return Builder.getInt1(value);
    } else if (lit.evaluatedType == ExprNode::DataType::String) {
      llvm::Value* globalStrPtr =
          Builder.CreateGlobalString(lit.value, ".str_lit");
      llvm::Value* strLength = Builder.getInt64(lit.value.length());
      llvm::Function* allocFn = M->getFunction("my_lang_alloc");
      if (!allocFn) {
        std::cerr
            << "Internal Compiler Error: my_lang_alloc not found in module!\n";
        return nullptr;
      }
      llvm::Value* runtimeMemPtr =
          Builder.CreateCall(allocFn, {strLength}, "runtime_mem");
      llvm::Function* memcpyFn = llvm::Intrinsic::getDeclarationIfExists(
          M, llvm::Intrinsic::memcpy,
          {Builder.getPtrTy(), Builder.getPtrTy(), Builder.getInt64Ty()});
      Builder.CreateCall(memcpyFn, {runtimeMemPtr, globalStrPtr, strLength,
                                    Builder.getInt1(false)});
      llvm::Type* ptrType = llvm::PointerType::getUnqual(Context);
      llvm::Type* lengthType = llvm::Type::getInt64Ty(Context);
      llvm::StructType* stringStructType =
          llvm::StructType::get(Context, {ptrType, lengthType});
      llvm::Value* strStruct = llvm::UndefValue::get(stringStructType);
      strStruct = Builder.CreateInsertValue(strStruct, runtimeMemPtr, 0,
                                            "str_struct_ptr");
      strStruct =
          Builder.CreateInsertValue(strStruct, strLength, 1, "str_struct_len");
      return strStruct;
    }
  }
  if (node->getType() == NodeType::variableUse) {
    auto& ref = static_cast<VariableNode&>(*node);
    std::string lookupName = ref.mangledName;
    if (NamedValues.find(lookupName) == NamedValues.end()) {
      lookupName = ref.name;
    }
    if (NamedValues.find(lookupName) == NamedValues.end()) {
      for (auto const& [key, val] : NamedValues) std::cerr << key << " ";
      std::cerr << "\n";
      exit(1);
    }

    llvm::Value* varPtr = NamedValues[lookupName];
    llvm::Type* llvmType = nullptr;
    if (auto* allocaInst = llvm::dyn_cast<llvm::AllocaInst>(varPtr)) {
      llvmType = allocaInst->getAllocatedType();
    } else {
      llvmType = getTy(ref.evaluatedType, Context);
      if (!llvmType || llvmType->isVoidTy())
        llvmType = llvm::Type::getInt32Ty(Context);
    }
    return Builder.CreateLoad(llvmType, varPtr, lookupName);
  }
  if (node->getType() == NodeType::collectionUse) {
    auto& ref = static_cast<CollectionUseNode&>(*node);
    auto it = NamedValues.find(ref.mangledName);
    if (it == NamedValues.end()) {
      std::cerr << "Internal Compiler Error: collection not found in codegen: "
                << ref.mangledName << "\n";
      return nullptr;
    }
  }
  if (node->getType() == NodeType::binnaryOp) {
    auto& op = static_cast<BinnaryOpNode&>(*node);
    llvm::Value* L = codegenExpr(op.left.get(), Builder, Context, M);
    llvm::Value* R = codegenExpr(op.right.get(), Builder, Context, M);

    if (!L || !R) return nullptr;
    bool isFloat = (op.evaluatedType == ExprNode::DataType::Float);
    if (op.op == Operator::plus) {
      return isFloat ? Builder.CreateFAdd(L, R, "addftmp")
                     : Builder.CreateAdd(L, R, "addtmp");
    }
    if (op.op == Operator::minus) {
      return isFloat ? Builder.CreateFSub(L, R, "subftmp")
                     : Builder.CreateSub(L, R, "subtmp");
    }
    if (op.op == Operator::multiply) {
      return isFloat ? Builder.CreateFMul(L, R, "mulftmp")
                     : Builder.CreateMul(L, R, "multmp");
    }
    if (op.op == Operator::divide) {
      return isFloat ? Builder.CreateFDiv(L, R, "divftmp")
                     : Builder.CreateSDiv(L, R,
                                          "divtmp");
    }

    if (op.op == Operator::lessThan) {
      return isFloat ? Builder.CreateFCmpOLT(L, R, "cmplttmp")
                     : Builder.CreateICmpSLT(L, R, "cmplttmp");
    }
    if (op.op == Operator::greaterThan) {
      return isFloat ? Builder.CreateFCmpOGT(L, R, "cmpgttmp")
                     : Builder.CreateICmpSGT(L, R, "cmpgttmp");
    }
    if (op.op == Operator::equal) {
      return isFloat ? Builder.CreateFCmpOEQ(L, R, "cmpeqtmp")
                     : Builder.CreateICmpEQ(L, R, "cmpeqtmp");
    }
    if (op.op == Operator::notEqual) {
      return isFloat ? Builder.CreateFCmpONE(L, R, "cmpnetmp")
                     : Builder.CreateICmpNE(L, R, "cmpnetmp");
    }
    if (op.op == Operator::lessOrEqual) {
      return isFloat ? Builder.CreateFCmpOLE(L, R, "cmpletmp")
                     : Builder.CreateICmpSLE(L, R, "cmpletmp");
    }
    if (op.op == Operator::greaterOrEqual) {
      return isFloat ? Builder.CreateFCmpOGE(L, R, "cmpgetmp")
                     : Builder.CreateICmpSGE(L, R, "cmpgetmp");
    }
  }
  if (node->getType() == NodeType::functionCall) {
    return buildFuncCall(node, Builder, Context, M);
  }
  return nullptr;
}

void codegenStatement(ASTNode* node, llvm::IRBuilder<>& Builder,
                      llvm::LLVMContext& Context, llvm::Module* M) {
  if (!node) return;

  if (node->getType() == NodeType::variableDeclr) {
    auto& var = static_cast<VariableDeclrNode&>(*node);
    llvm::Type* type = nullptr;
    if (var.expression->evaluatedType == ExprNode::DataType::Int) {
      type = llvm::Type::getInt32Ty(Context);
    } else if (var.expression->evaluatedType == ExprNode::DataType::Float) {
      type = llvm::Type::getDoubleTy(Context);
    } else if (var.expression->evaluatedType == ExprNode::DataType::Bool) {
      type = llvm::Type::getInt1Ty(Context);
    } else if (var.expression->evaluatedType == ExprNode::DataType::String) {
      llvm::Type* ptrType = llvm::PointerType::getUnqual(Context);
      llvm::Type* lengthType = llvm::Type::getInt64Ty(Context);
      type = llvm::StructType::get(Context, {ptrType, lengthType});
    }
    std::string insertName = var.mangledName;
    if (var.mangledName.empty()) {
      insertName = var.name;
    }
    llvm::AllocaInst* allocaInst =
        Builder.CreateAlloca(type, nullptr, insertName);
    NamedValues[var.mangledName] = allocaInst;

    llvm::Value* initValue =
        codegenExpr(var.expression.get(), Builder, Context, M);
    if (initValue) {
      Builder.CreateStore(initValue, allocaInst);
    }
  }
  if (node->getType() == NodeType::variableOverride) {
    auto& var = static_cast<VariableAssignmentNode&>(*node);
    auto it = NamedValues.find(var.mangledName);
    if (it == NamedValues.end()) {
      std::cerr << "Internal Compiler Error: variable not found for override: "
                << var.mangledName << "\n";
      return;
    }
    llvm::Value* varPtr =
        it->second;
    llvm::Value* newValue =
        codegenExpr(var.expression.get(), Builder, Context, M);
    if (!newValue) return;
    if (var.expression->evaluatedType == ExprNode::DataType::Int ||
        var.expression->evaluatedType == ExprNode::DataType::Float ||
        var.expression->evaluatedType == ExprNode::DataType::Bool) {
      Builder.CreateStore(newValue, varPtr);
    }
    else if (var.expression->evaluatedType == ExprNode::DataType::String) {
      llvm::Function* allocFn = M->getFunction("my_lang_alloc");
      llvm::Function* freeFn = M->getFunction("my_lang_free");
      llvm::Function* memcpyFn = llvm::Intrinsic::getDeclarationIfExists(
          M, llvm::Intrinsic::memcpy,
          {Builder.getPtrTy(), Builder.getPtrTy(), Builder.getInt64Ty()});

      if (!allocFn || !freeFn) {
        std::cerr << "Internal Compiler Error: String runtime functions "
                     "missing in module!\n";
        return;
      }
      llvm::Value* newBytesPtr =
          Builder.CreateExtractValue(newValue, 0, "new_bytes_ptr");
      llvm::Value* newLength =
          Builder.CreateExtractValue(newValue, 1, "new_len");
      llvm::Value* newRuntimeMem =
          Builder.CreateCall(allocFn, {newLength}, "new_runtime_mem");
      Builder.CreateCall(memcpyFn, {newRuntimeMem, newBytesPtr, newLength,
                                    Builder.getInt1(false)});
      llvm::Type* ptrType = llvm::PointerType::getUnqual(Context);
      llvm::Type* lengthType = llvm::Type::getInt64Ty(Context);
      llvm::StructType* stringStructType =
          llvm::StructType::get(Context, {ptrType, lengthType});
      llvm::Value* oldPtrAddr =
          Builder.CreateStructGEP(stringStructType, varPtr, 0, "old_ptr_addr");
      llvm::Value* oldLenAddr =
          Builder.CreateStructGEP(stringStructType, varPtr, 1, "old_len_addr");
      llvm::Value* oldPtr =
          Builder.CreateLoad(Builder.getPtrTy(), oldPtrAddr, "old_ptr");
      llvm::Value* oldLen =
          Builder.CreateLoad(Builder.getInt64Ty(), oldLenAddr, "old_len");
      Builder.CreateStore(newRuntimeMem, oldPtrAddr);
      Builder.CreateStore(newLength, oldLenAddr);
      Builder.CreateCall(freeFn, {oldPtr, oldLen});
    }

    return;
  }
  if (node->getType() == NodeType::ifStatement) {
    auto& statement = static_cast<IfStatementNode&>(*node);
    llvm::Function* TheFunction = Builder.GetInsertBlock()->getParent();
    llvm::Value* Cond =
        codegenExpr(statement.condition.get(), Builder, Context, M);
    llvm::BasicBlock* ThenBB =
        llvm::BasicBlock::Create(Context, "then", TheFunction);
    llvm::BasicBlock* ElseBB =
        statement.orelse ? llvm::BasicBlock::Create(Context, "else") : nullptr;
    llvm::BasicBlock* MergeBB = nullptr;
    auto GetOrCreateMergeBB = [&]() {
      if (!MergeBB) {
        MergeBB = llvm::BasicBlock::Create(Context, "ifcont");
      }
      return MergeBB;
    };
    if (ElseBB) {
      Builder.CreateCondBr(Cond, ThenBB, ElseBB);
    } else {
      Builder.CreateCondBr(Cond, ThenBB, GetOrCreateMergeBB());
    }
    Builder.SetInsertPoint(ThenBB);
    codegenStatement(statement.thenBlock.get(), Builder, Context, M);
    if (!Builder.GetInsertBlock()->getTerminator()) {
      Builder.CreateBr(GetOrCreateMergeBB());
    }
    if (ElseBB) {
      TheFunction->insert(TheFunction->end(), ElseBB);
      Builder.SetInsertPoint(ElseBB);

      codegenStatement(statement.orelse.get(), Builder, Context, M);

      if (!Builder.GetInsertBlock()->getTerminator()) {
        Builder.CreateBr(GetOrCreateMergeBB());
      }
    }
    if (MergeBB && (!MergeBB->use_empty() || ElseBB == nullptr)) {
      TheFunction->insert(TheFunction->end(), MergeBB);
      Builder.SetInsertPoint(MergeBB);
    } else if (MergeBB) {
      delete MergeBB;
    }
  }
  if (node->getType() == NodeType::forStatement) {
    auto& forNode = static_cast<ForStatementNode&>(*node);
    if (std::holds_alternative<ForStatementNode::Range>(forNode.source)) {
      ForStatementNode::Range range = std::get<0>(std::move(forNode.source));
      llvm::Type* intType = Builder.getInt32Ty();
      llvm::AllocaInst* allocaI =
          Builder.CreateAlloca(intType, nullptr, forNode.mangledName);
      Builder.CreateStore(codegenExpr(range.from.get(), Builder, Context, M),
                          allocaI);
      llvm::Function* current_func = Builder.GetInsertBlock()->getParent();
      llvm::BasicBlock* condBB =
          llvm::BasicBlock::Create(Context, "loop.cond", current_func);
      llvm::BasicBlock* bodyBB =
          llvm::BasicBlock::Create(Context, "loop.body", current_func);
      llvm::BasicBlock* exitBB =
          llvm::BasicBlock::Create(Context, "loop.exit", current_func);

      Builder.CreateBr(condBB);
      Builder.SetInsertPoint(condBB);

      llvm::Value* valI = Builder.CreateLoad(intType, allocaI, "load.i");
      llvm::Value* cond = Builder.CreateICmpSLT(
          valI, codegenExpr(range.to.get(), Builder, Context, M), "cmp.cond");
      Builder.CreateCondBr(cond, bodyBB, exitBB);

      Builder.SetInsertPoint(bodyBB);

      for (auto& node : forNode.body->block) {
        if (node->getType() == NodeType::break_) {
          Builder.CreateBr(exitBB);
        } else {
          codegenStatement(node.get(), Builder, Context, M);
        }
      }

      /*llvm::BasicBlock* currentBodyEndBB =*/ Builder.GetInsertBlock();

      llvm::Value* currentI = Builder.CreateLoad(intType, allocaI);
      llvm::Value* nextI = Builder.CreateAdd(
          currentI, codegenExpr(range.step.get(), Builder, Context, M),
          forNode.mangledName);
      Builder.CreateStore(nextI, allocaI);
      Builder.CreateBr(condBB);
      Builder.SetInsertPoint(exitBB);
    } else {
      return;
    }
  }
  if (node->getType() == NodeType::whileStatement) {
    auto& whileNode = static_cast<WhileStatementNode&>(*node);
    llvm::Function* currentFunc = Builder.GetInsertBlock()->getParent();
    llvm::BasicBlock* condBB =
        llvm::BasicBlock::Create(Context, "while.cond", currentFunc);
    llvm::BasicBlock* bodyBB =
        llvm::BasicBlock::Create(Context, "while.body", currentFunc);
    llvm::BasicBlock* exitBB =
        llvm::BasicBlock::Create(Context, "while.exit", currentFunc);

    Builder.CreateBr(condBB);
    Builder.SetInsertPoint(condBB);

    llvm::Value* cond =
        codegenExpr(whileNode.condition.get(), Builder, Context, M);

    Builder.CreateCondBr(cond, bodyBB, exitBB);

    Builder.SetInsertPoint(bodyBB);
    for (auto& stmntNode : whileNode.body->block) {
      if (stmntNode->getType() == NodeType::break_) {
        Builder.CreateBr(exitBB);
      } else {
        codegenStatement(stmntNode.get(), Builder, Context, M);
      }
    }
    Builder.CreateBr(condBB);
    Builder.SetInsertPoint(exitBB);
  }
  if (node->getType() == NodeType::functionCall) {
    auto& func = static_cast<FunctionCallNode&>(*node);
    llvm::Type* Ty = getTy(func.evaluatedType, Context);
    std::vector<llvm::Type*> argTy;
    for (auto& arg : func.Args) {
      argTy.push_back(getTy(arg->evaluatedType, Context));
    }
    llvm::FunctionType* FuncTy =
        llvm::FunctionType::get(Ty,
                                argTy,
                                false
        );
    std::string insertName = func.mangledName;
    if (func.mangledName.empty()) {
      insertName = func.functionName;
    }
    llvm::FunctionCallee Func = M->getOrInsertFunction(insertName, FuncTy);
    std::vector<llvm::Value*> Args;
    for (auto& arg : func.Args) {
      Args.push_back(codegenExpr(arg.get(), Builder, Context, M));
    }

    /*llvm::CallInst* Call =*/ Builder.CreateCall(Func, Args);
  }
  if (node->getType() == NodeType::returns) {
    auto& ret = static_cast<ReturnNode&>(*node);
    llvm::Value* retVal = codegenExpr(ret.returns.get(), Builder, Context, M);
    llvm::Function* currentFunc = Builder.GetInsertBlock()->getParent();
    bool isMain = (currentFunc && currentFunc->getName() == "main");

    if (retVal) {
      Builder.CreateRet(retVal);
    } else {
      if (isMain) {
        Builder.CreateRet(Builder.getInt32(0));
      } else {
        Builder.CreateRetVoid();
      }
    }
  }
}

void compile(NodeArray& nrr, [[maybe_unused]] int targetPlatform, [[maybe_unused]] int flags) {
  llvm::LLVMContext Context;
  llvm::Module* M = new llvm::Module("base_module", Context);
  llvm::IRBuilder<> Builder(Context);

  llvm::StructType* stringType =
      llvm::StructType::create(Context, "StringView");
  stringType->setBody({
      Builder.getPtrTy(),
      Builder.getInt64Ty()
  });

  for (const auto& node : nrr) {
    if (node->getType() == NodeType::main) {
      auto& mainNode = static_cast<MainNode&>(*node);
      llvm::FunctionType* FuncType =
          llvm::FunctionType::get(Builder.getInt32Ty(), false);
      llvm::Function* MainFunc = llvm::Function::Create(
          FuncType, llvm::Function::ExternalLinkage, "main", M);

      llvm::BasicBlock* MainBB =
          llvm::BasicBlock::Create(Context, "entry", MainFunc);
      llvm::BasicBlock* savedBB = Builder.GetInsertBlock();
      Builder.SetInsertPoint(MainBB);

      for (auto& inside : mainNode.logic) {
        codegenStatement(inside.get(), Builder, Context, M);
      }

      if (!Builder.GetInsertBlock()->getTerminator()) {
        Builder.CreateRet(Builder.getInt32(0));
      }

      if (savedBB) {
        Builder.SetInsertPoint(savedBB);
      } else {
        Builder.ClearInsertionPoint();
      }
    }
    if (node->getType() == NodeType::functionDef) {
      auto& fun = static_cast<FunctionDeclNode&>(*node);

      std::vector<llvm::Type*> argTypes;
      std::vector<std::string>
          validMangledNames;

      for (auto& param : fun.Params) {
        if (param.name.empty() && param.mangledName.empty()) {
          continue;
        }

        llvm::Type* t = getTy(param.type, Context);
        if (!t || t->isVoidTy()) {
          t = llvm::Type::getInt32Ty(Context);
        }
        argTypes.push_back(t);
        std::string finalName = param.mangledName.empty()
                                    ? (fun.mangledName + ":" + param.name)
                                    : param.mangledName;
        validMangledNames.push_back(finalName);
      }
      llvm::FunctionType* tyep = llvm::FunctionType::get(
          getTy(fun.evaluatedType, Context), argTypes, false);
      llvm::Function* func = llvm::Function::Create(
          tyep, llvm::Function::ExternalLinkage, fun.mangledName, M);
      if (fun.isExtern || !fun.does) {
        continue;
      }
      llvm::BasicBlock* savedBB = Builder.GetInsertBlock();
      llvm::BasicBlock* block =
          llvm::BasicBlock::Create(Context, "entry", func);
      Builder.SetInsertPoint(block);
      auto savedNamedValues = NamedValues;
      unsigned int argIdx = 0;
      for (auto& arg : func->args()) {
        std::string currentParamName = validMangledNames[argIdx];

        arg.setName(currentParamName);
        llvm::AllocaInst* allocaInst =
            Builder.CreateAlloca(arg.getType(), nullptr, currentParamName);
        Builder.CreateStore(&arg, allocaInst);
        NamedValues[currentParamName] = allocaInst;

        argIdx++;
      }
      if (fun.does && fun.does->getType() == NodeType::block) {
        auto& bodyBlock = static_cast<Block&>(*fun.does);
        for (const auto& insideNode : bodyBlock.block) {
          codegenStatement(insideNode.get(), Builder, Context, M);
        }
      }
      if (!Builder.GetInsertBlock()->getTerminator()) {
        if (fun.evaluatedType == ExprNode::DataType::Int) {
          Builder.CreateRet(Builder.getInt32(0));
        } else if (fun.evaluatedType == ExprNode::DataType::Float) {
          Builder.CreateRet(llvm::ConstantFP::get(Context, llvm::APFloat(0.0)));
        } else if (fun.evaluatedType == ExprNode::DataType::Bool) {
          Builder.CreateRet(Builder.getInt1(false));
        } else {
          Builder.CreateRetVoid();
        }
      }
      NamedValues = savedNamedValues;
      if (savedBB) {
        Builder.SetInsertPoint(savedBB);
      } else {
        Builder.ClearInsertionPoint();
      }
    }
  }
  
  std::string verifyError;
  llvm::raw_string_ostream errorStream(verifyError);
  
  bool hasErrors = llvm::verifyModule(*M, &errorStream);
  
  if (hasErrors) {
     std::cerr << "\033[31mCRITICAL ERROR. IR VERIFICATION FAILED\033[0m\n" << errorStream.str() << "\n";
     return; 
  }

  std::error_code EC;

  llvm::raw_fd_ostream file("debug.ll", EC, llvm::sys::fs::OF_None);
  if (EC) {
    std::cerr << "Couldn't open file to write: " << EC.message() << ".\n";
    delete M;
    return;
  }

  M->print(file, nullptr);

  file.flush();

  llvm::InitializeNativeTarget();
  llvm::InitializeNativeTargetAsmPrinter();
  llvm::InitializeNativeTargetAsmParser();

  auto TargetTripleStr = llvm::sys::getProcessTriple();
  llvm::Triple TargetTriple(TargetTripleStr);
  M->setTargetTriple(TargetTriple);

  std::string Error;
  auto Target = llvm::TargetRegistry::lookupTarget(TargetTriple, Error);
  if (!Target) {
      std::cerr << "TargetRegistry Error: " << Error << "\n";
      delete M;
      return;
  }

  auto CPU = "generic";
  auto Features = "";
  llvm::TargetOptions opt;
  std::optional<llvm::Reloc::Model> RM;
  auto TargetMachine = Target->createTargetMachine(TargetTriple, CPU, Features, opt, RM);

  M->setDataLayout(TargetMachine->createDataLayout());

  std::error_code EC_obj;
  llvm::raw_fd_ostream dest("output.o", EC_obj, llvm::sys::fs::OF_None);
  if (EC_obj) {
      std::cerr << "Couldn't open file to write object: " << EC_obj.message() << ".\n";
      delete M;
      return;
  }

  llvm::legacy::PassManager pass;
  llvm::CodeGenFileType FileType = llvm::CodeGenFileType::ObjectFile;


  if (TargetMachine->addPassesToEmitFile(pass, dest, nullptr, FileType)) {
      std::cerr << "TargetMachine can't emit an object file\n";
      delete M;
      return;
  }

  pass.run(*M);
  dest.flush();

  // Just Compile it With Clang

  delete M;
}
