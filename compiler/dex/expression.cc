/*
 * Copyright (C) 2014 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0(the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "expression.h"
#include "mir_graph.h"

namespace art {

void UnaryExpression::GetChildren(std::vector<Expression*>& children) const {
  children.push_back(operand_);
}

void BinaryExpression::GetChildren(std::vector<Expression*>& children) const {
  children.push_back(lhs_);
  children.push_back(rhs_);
}

std::string BinaryExpression::ToString(const CompilationUnit* c_unit) {
  std::string expression_string;

  expression_string.append("(");
  expression_string.append(assignment_to_->ToString(c_unit));
  expression_string.append(" = ");

  if (exp_kind_ == ExpKind_Phi) {
    expression_string.append("PHI");
  }

  expression_string.append("(");

  expression_string.append(lhs_->ToString(c_unit));

  switch (exp_kind_) {
    case ExpKind_ConstSet:
      break;
    case ExpKind_Add:
      expression_string.append(" + ");
      break;
    case ExpKind_Sub:
      expression_string.append(" - ");
      break;
    case ExpKind_Mul:
      expression_string.append(" * ");
      break;
    case ExpKind_Phi:
      expression_string.append(", ");
      break;
    case ExpKind_Div:
      expression_string.append(" / ");
      break;
    case ExpKind_Rem:
      expression_string.append(" % ");
      break;
    case ExpKind_And:
      expression_string.append(" & ");
      break;
    case ExpKind_Or:
      expression_string.append(" | ");
      break;
    case ExpKind_Xor:
      expression_string.append(" ^ ");
      break;
    case ExpKind_Shl:
      expression_string.append(" << ");
      break;
    case ExpKind_Shr:
      expression_string.append(" >> ");
      break;
    case ExpKind_Ushr:
      expression_string.append(" >>> ");
      break;
    case ExpKind_Invalid:
    default:
      expression_string.append(" ?? ");
      break;
  }

  expression_string.append(rhs_->ToString(c_unit));
  expression_string.append("))");

  return expression_string;
}

std::string UnaryExpression::ToString(const CompilationUnit* c_unit) {
  std::string expression_string;

  expression_string.append("(");
  expression_string.append(assignment_to_->ToString(c_unit));
  expression_string.append(" = ");

  if (exp_kind_ == ExpKind_Cast) {
    expression_string.append("(cast)");
  } else if (exp_kind_ == ExpKind_Invalid) {
    expression_string.append(" ?? ");
  }

  expression_string.append(operand_->ToString(c_unit));
  expression_string.append(")");

  return expression_string;
}

std::string VirtualRegister::ToString(const CompilationUnit* c_unit) {
  std::stringstream ss;

  // For readability, we convert to dalvik register.
  int low_dalvik_reg = c_unit->mir_graph->SRegToVReg(low_ssa_reg_);

  ss << "v" << low_dalvik_reg;

  // If our virtual register is wide, we print the VR for the high bits.
  if (IsWide() == true) {
    int high_dalvik_reg = c_unit->mir_graph->SRegToVReg(high_ssa_reg_);

    ss << ", v" << high_dalvik_reg;
  }

  return ss.str();
}

std::string ConstantExpression::ToString(const CompilationUnit* c_unit) {
  std::stringstream ss;

  // Since we don't know how the constant will be interpreted, we always
  // print its 64-bit int representation.
  ss << GetValue<int64_t>();

  return ss.str();
}

Expression* Expression::MirToExpression(ArenaAllocator* arena, MIR* mir,
                                        std::map<VirtualRegister*, Expression*> * vr_to_expression) {
  if (mir == nullptr) {
    return nullptr;
  }

  Instruction::Code opcode = mir->dalvikInsn.opcode;

  Expression* result = 0;

  // In order to figure out how to create the expression, we look at flags
  // from the dataflow information. However, a better approach here would be
  // to update opcode-gen to automatically generate tables with this
  // information for every bytecode. That can only work when expression
  // implementation is complete.

  int flags = Instruction::FlagsOf(opcode);
  uint64_t df_flags = MIRGraph::GetDataFlowAttributes(opcode);

  ExpressionKind exp_kind = ExpKind_Invalid;

  if (flags & Instruction::kAdd) {
    exp_kind = ExpKind_Add;
  } else if (flags & Instruction::kSubtract) {
    exp_kind = ExpKind_Sub;
  } else if (flags & Instruction::kMultiply) {
    exp_kind = ExpKind_Mul;
  } else if (flags & Instruction::kDivide) {
    exp_kind = ExpKind_Div;
  } else if (flags & Instruction::kRemainder) {
    exp_kind = ExpKind_Rem;
  } else if (flags & Instruction::kAnd) {
    exp_kind = ExpKind_And;
  } else if (flags & Instruction::kOr) {
    exp_kind = ExpKind_Or;
  } else if (flags & Instruction::kXor) {
    exp_kind = ExpKind_Xor;
  } else if (flags & Instruction::kShr) {
    exp_kind = ExpKind_Shr;
  } else if (flags & Instruction::kShl) {
    exp_kind = ExpKind_Shl;
  } else if (flags & Instruction::kUshr) {
    exp_kind = ExpKind_Ushr;
  } else if (flags & Instruction::kCast) {
    exp_kind = ExpKind_Cast;
  } else if (df_flags & DF_SETS_CONST) {
    exp_kind = ExpKind_ConstSet;
  } else if (opcode == static_cast<Instruction::Code>(kMirOpPhi)) {
    exp_kind = ExpKind_Phi;
  }

  switch (exp_kind) {
    case ExpKind_ConstSet:
    case ExpKind_Cast:
      result = UnaryExpression::MirToExpression(arena, mir, vr_to_expression, exp_kind);
      break;
    case ExpKind_Invalid:
      // Do nothing for invalids.
      break;
    default:
      result = BinaryExpression::MirToExpression(arena, mir, vr_to_expression, exp_kind);
      break;
  }

  return result;
}

std::map<MIR*, Expression*> Expression::MirsToExpressions(ArenaAllocator* arena,
                                                          const std::vector<MIR*> & listOfMirs) {
  std::map<VirtualRegister*, Expression*> vr_to_expression;
  std::map<MIR*, Expression*> mir_to_expression_map;

  for (std::vector<MIR*>::const_iterator iter = listOfMirs.begin(); iter != listOfMirs.end(); iter++) {
    MIR* mir = (*iter);

    // Convert mir to expression.
    Expression* result = MirToExpression(arena, mir, &vr_to_expression);

    // We insert the mapping into the map. Note that null expression
    // is allowed to be inserted.
    mir_to_expression_map.insert(std::pair<MIR*, Expression*>(mir, result));
  }

  return mir_to_expression_map;
}

BinaryExpression* BinaryExpression::MirToExpression(ArenaAllocator* arena, MIR* mir,
                                                    std::map<VirtualRegister*, Expression*> * vr_to_expression,
                                                    ExpressionKind exp_kind_) {
  if (mir == nullptr) {
    return nullptr;
  }

  // Get local copy of ssa representation.
  SSARepresentation* ssa_rep = mir->ssa_rep;

  // We cannot build the expression for assignment to without ssa rep.
  if (ssa_rep == nullptr) {
    return nullptr;
  }

  // If we don't have at least one definition we cannot create an assignment
  // expression.
  if (ssa_rep->num_defs <= 0 || ssa_rep->defs == nullptr) {
    return nullptr;
  }

  // The results of all binary expressions must be assigned to a VR so we create
  // an expression for that first. We must create the VR and thus we don't need
  // a map to look for an expression tree.
  VirtualRegister* assign_to = nullptr;
  Expression* assign_to_expr = nullptr;

  {
    bool is_wide = (ssa_rep->num_defs == 1) ? false : true;
    int low_ssa_reg = ssa_rep->defs[0];
    int high_ssa_reg = is_wide ? ssa_rep->defs[1] : 0;

    assign_to_expr = VirtualRegister::ConvertToVR(arena, nullptr, low_ssa_reg, high_ssa_reg, is_wide);
  }

  if (assign_to_expr != nullptr) {
    // Because of how we built it, it should be a virtual register but we
    // are just being paranoid.
    if (assign_to_expr->IsVirtualRegister() == true) {
      assign_to = static_cast<VirtualRegister*>(assign_to_expr);
    }
  }

  // If we fail at creating the VR we are assigning to, then we cannot
  // complete successfully.
  if (assign_to == nullptr) {
    return nullptr;
  }

  Expression* lhs = nullptr, *rhs = nullptr;

  // Since we are generating for a binary expression, there must be two operands.
  // Thus we look at the number of uses to figure out which of the following
  // scenarios we are dealing with:
  // 1. One use means one operand is non-wide VR and other is literal.
  // 2. Two uses means both operands are non-wide VRs.
  // 3. Three uses means first operand is wide VRs while the second one is non-wide VR.
  // 4. Four uses means both operands are wide VRs.

  if (ssa_rep->num_uses == 1) {
    DCHECK(ssa_rep->uses != nullptr);

    lhs = VirtualRegister::ConvertToVR(arena, vr_to_expression, ssa_rep->uses[0]);

    int literalValue = mir->dalvikInsn.vC;
    rhs = ConstantExpression::NewExpression(arena, literalValue);
  } else if (ssa_rep->num_uses == 2) {
    DCHECK(ssa_rep->uses != nullptr);

    lhs = VirtualRegister::ConvertToVR(arena, vr_to_expression, ssa_rep->uses[0]);
    rhs = VirtualRegister::ConvertToVR(arena, vr_to_expression, ssa_rep->uses[1]);
  } else if (ssa_rep->num_uses == 3) {
    DCHECK(ssa_rep->uses != nullptr);

    lhs = VirtualRegister::ConvertToVR(arena, vr_to_expression, ssa_rep->uses[0], ssa_rep->uses[1], true);
    rhs = VirtualRegister::ConvertToVR(arena, vr_to_expression, ssa_rep->uses[2]);
  } else if (ssa_rep->num_uses == 4) {
    DCHECK(ssa_rep->uses != nullptr);

    lhs = VirtualRegister::ConvertToVR(arena, vr_to_expression, ssa_rep->uses[0], ssa_rep->uses[1], true);
    rhs = VirtualRegister::ConvertToVR(arena, vr_to_expression, ssa_rep->uses[2], ssa_rep->uses[3], true);
  } else {
    // An assumption we made must be wrong if we get here. In DCHECK world
    // we want to fail if we get here.
    DCHECK(false);

    return nullptr;
  }

  // If we did not generate operands successfully, then we cannot fully
  // generate the expression.
  if (lhs == nullptr || rhs == nullptr) {
    return nullptr;
  }

  BinaryExpression* result = nullptr;

  // Now we put toGether the operands to create a binary expression.
  if (exp_kind_ != ExpKind_Invalid) {
    // In order to create expression, we must first find out the
    // primitive type of the result.
    ExpressionType exp_type = BytecodeExpression::GetExpressionType(mir->dalvikInsn.opcode);

    // If we know the type, we can create the binary expression.
    if (exp_type != ExpType_Invalid) {
      result = BinaryExpression::NewExpression(arena, mir, assign_to, lhs, rhs, exp_kind_, exp_type);
    }
  }

  // If we have created an expression, we should add its tree to the mapping
  // of VRs to Expressions.
  if (result != nullptr && vr_to_expression != nullptr) {
    vr_to_expression->insert(std::pair<VirtualRegister*, Expression*>(assign_to, result));
  }

  return result;
}

UnaryExpression* UnaryExpression::MirToExpression(ArenaAllocator* arena, MIR* mir,
                                                  std::map<VirtualRegister*, Expression*> * vr_to_expression,
                                                  ExpressionKind exp_kind) {
  if (mir == nullptr) {
    return nullptr;
  }

  // Get local copy of the ssa representation.
  SSARepresentation* ssa_rep = mir->ssa_rep;

  // We cannot build the expression for assignment to without ssa rep.
  if (ssa_rep == nullptr) {
    return nullptr;
  }

  // If we don't have at least one definition we cannot create an assignment
  // expression.
  if (ssa_rep->num_defs <= 0 || ssa_rep->defs == nullptr) {
    return 0;
  }

  // The results of all unary expressions must be assigned to a VR so we create
  // an expression for that first. We must create the VR and thus we don't need
  // a map to look for an expression tree.
  VirtualRegister* assign_to = 0;
  Expression* assign_to_expr = 0;

  {
    bool is_wide = (ssa_rep->num_defs == 1) ? false : true;
    int low_ssa_reg = ssa_rep->defs[0];
    int high_ssa_reg = is_wide ? ssa_rep->defs[1] : 0;

    assign_to_expr = VirtualRegister::ConvertToVR(arena, nullptr, low_ssa_reg, high_ssa_reg, is_wide);
  }

  if (assign_to_expr != nullptr) {
    // Because of how we built it, it should be a virtual register but we
    // are just being paranoid.
    if (assign_to_expr->IsVirtualRegister() == true) {
      assign_to = static_cast<VirtualRegister*>(assign_to_expr);
    }
  }

  // If we fail at creating the VR we are assigning to, then we cannot
  // complete successfully.
  if (assign_to == nullptr) {
    return nullptr;
  }

  Expression* operand = 0;

  // Since we are generating for a unary expression, there must be one operand.
  // Thus we look at the number of uses to figure out which of the following
  // scenarios we are dealing with:
  // 1. Zero uses mean that we are dealing with either wide or non-wide constant.
  // 2. One use means that operand is a non-wide VR.
  // 3. Two uses means that operand is a wide VR.

  if (ssa_rep->num_uses == 0) {
    bool is_wide = false;
    int64_t value = 0;

    // Get the constant value.
    bool setsConst = mir->dalvikInsn.GetConstant(&value, &is_wide);

    // If we have a constant set expression, then we can build
    // a constant expression for the operand.
    if (setsConst == true) {
      operand = ConstantExpression::NewExpression(arena, value, is_wide);
    }
  } else if (ssa_rep->num_uses == 1) {
    DCHECK(ssa_rep->uses != nullptr);

    operand = VirtualRegister::ConvertToVR(arena, vr_to_expression, ssa_rep->uses[0]);
  } else if (ssa_rep->num_uses == 2) {
    DCHECK(ssa_rep->uses != nullptr);

    operand = VirtualRegister::ConvertToVR(arena, vr_to_expression, ssa_rep->uses[0], ssa_rep->uses[1], true);
  } else {
    // An assumption we made must be wrong if we get here. In DCHECK world
    // we want to fail if we get here.
    DCHECK(false);

    return 0;
  }

  // If we did not generate operands successfully, then we cannot fully
  // generate the expression.
  if (operand == nullptr) {
    return nullptr;
  }

  UnaryExpression* result = 0;

  // Now we need to create expressions for the operands of the binary expression
  // we are generating.
  if (exp_kind != ExpKind_Invalid) {
    ExpressionType exp_type = BytecodeExpression::GetExpressionType(mir->dalvikInsn.opcode);

    // Some of the unary expressions have unknown type until a use.
    // For example, this applies to const bytecodes. Thus we do not
    // check whether exp_type is invalid.

    result = UnaryExpression::NewExpression(arena, mir, assign_to, operand, exp_kind, exp_type);
  }

  // If we have created an expression, we should add its tree to the mapping
  // of VRs to Expressions.
  if (result != nullptr && vr_to_expression != nullptr) {
    vr_to_expression->insert(std::pair<VirtualRegister*, Expression*>(assign_to, result));
  }

  return result;
}

BinaryExpression* BinaryExpression::NewExpression(ArenaAllocator* arena, MIR* mir, VirtualRegister* assign_to,
                                                  Expression* lhs, Expression * rhs, ExpressionKind exp_kind_,
                                                  ExpressionType exp_type) {
  // If we don't have all parts of expression, we cannot create it.
  if (mir == nullptr || assign_to == nullptr || lhs == nullptr || rhs == nullptr) {
    return nullptr;
  }

  BinaryExpression* result = new (arena) BinaryExpression(assign_to, lhs, rhs, exp_kind_, exp_type, mir);
  return result;
}

UnaryExpression* UnaryExpression::NewExpression(ArenaAllocator* arena, MIR* mir, VirtualRegister* assign_to,
                                                Expression* operand, ExpressionKind exp_kind,
                                                ExpressionType exp_type) {
  // If we don't have all parts of expression, we cannot create it.
  if (mir == nullptr || assign_to == nullptr || operand == nullptr) {
    return nullptr;
  }

  UnaryExpression* result = new (arena) UnaryExpression(assign_to, operand, exp_kind, exp_type, mir);
  return result;
}

ConstantExpression* ConstantExpression::NewExpression(ArenaAllocator* arena, int64_t value, bool wide) {
  return new (arena) ConstantExpression(value, wide);
}

Expression* VirtualRegister::ConvertToVR(ArenaAllocator* arena,
                                         std::map<VirtualRegister*, Expression*>* vr_to_expression, int low_ssa_reg,
                                         int high_ssa_reg, bool wide) {
  VirtualRegister* result = nullptr;

  if (wide == false) {
    result = new (arena) VirtualRegister(low_ssa_reg);
  } else {
    result = new (arena) VirtualRegister(low_ssa_reg, high_ssa_reg);
  }

  // Look to see if we have an existing expression for this VR.
  Expression* existingExpression = result->FindExpressionForVR(vr_to_expression);

  // If we have an existing expression, return that instead.
  if (existingExpression != nullptr) {
    return existingExpression;
  } else {
    return result;
  }
}

Expression* VirtualRegister::FindExpressionForVR(std::map<VirtualRegister*, Expression*> * vr_to_expression) {
  // If the mapping doesn't exist, then there is nothing to find.
  if (vr_to_expression == nullptr) {
    return nullptr;
  }

  std::map<VirtualRegister*, Expression*>::const_iterator iter;

  // Look for an expression for the VR.
  iter = std::find_if(vr_to_expression->begin(), vr_to_expression->end(), VirtualRegisterMappingComparator(this));

  // If we didn't find an expression in the map for this VR, return 0.
  // Otherwise, return the expression we found.
  if (iter == vr_to_expression->end()) {
    return nullptr;
  } else {
    return iter->second;
  }
}

bool VirtualRegisterMappingComparator::operator()(
    const std::pair<VirtualRegister*, Expression*> vr_to_expressionMapping) {
  VirtualRegister* toCompareWith = vr_to_expressionMapping.first;

  if (toCompareWith == nullptr) {
    return false;
  }

  // If wideness does not match, the VRs are not equal.
  if (holder_->IsWide() != toCompareWith->IsWide()) {
    return false;
  }

  // If low ssa reg does not match, then the VRs are not equal.
  if (holder_->low_ssa_reg_ != toCompareWith->low_ssa_reg_) {
    return false;
  }

  // If high ssa reg does not match, then the VRs are not equal.
  if (holder_->high_ssa_reg_ != toCompareWith->high_ssa_reg_) {
    return false;
  }

  // If we made it this far, must be that the VRs are equal.
  return true;
}

MIR* BytecodeExpression::CreateMir(ArenaAllocator* arena, ExpressionKind exp_kind, ExpressionType exp_type,
                                   int assign_to_vr, int lhs_vr, int rhs_vr) {
  // This method supports only non-wide VRs and thus only supports
  // creating float and int MIRs.
  if (exp_type != ExpType_Int && exp_type != ExpType_Float) {
    return nullptr;
  }

  // Assume we will find an opcode we can use.
  bool found_opcode = true;
  Instruction::Code opcode;

  // As an enhancement to this logic, we could also allow 2addr forms
  // to be used.
  switch (exp_kind) {
    case ExpKind_Add:
      opcode = (exp_type == ExpType_Int) ? Instruction::ADD_INT : Instruction::ADD_FLOAT;
      break;
    case ExpKind_Sub:
      opcode = (exp_type == ExpType_Int) ? Instruction::SUB_INT : Instruction::SUB_FLOAT;
      break;
    case ExpKind_Mul:
      opcode = (exp_type == ExpType_Int) ? Instruction::MUL_INT : Instruction::MUL_FLOAT;
      break;
    default:
      found_opcode = false;
      break;
  }

  // If we didn't find an opcode we cannot generate a MIR.
  if (found_opcode == false) {
    return nullptr;
  }

  // Create the MIR and assign the fields.
  MIR* mir = new (arena) MIR();
  mir->dalvikInsn.opcode = opcode;
  mir->dalvikInsn.vA = assign_to_vr;
  mir->dalvikInsn.vB = lhs_vr;
  mir->dalvikInsn.vC = rhs_vr;

  return mir;
}

ExpressionType BytecodeExpression::GetExpressionType(Instruction::Code dalvik_opcode) {
  ExpressionType exp_type;

  switch (dalvik_opcode) {
    case Instruction::NEG_INT:
    case Instruction::NOT_INT:
    case Instruction::LONG_TO_INT:
    case Instruction::FLOAT_TO_INT:
    case Instruction::DOUBLE_TO_INT:
    case Instruction::INT_TO_BYTE:
    case Instruction::INT_TO_CHAR:
    case Instruction::INT_TO_SHORT:
    case Instruction::ADD_INT:
    case Instruction::SUB_INT:
    case Instruction::MUL_INT:
    case Instruction::DIV_INT:
    case Instruction::REM_INT:
    case Instruction::AND_INT:
    case Instruction::OR_INT:
    case Instruction::XOR_INT:
    case Instruction::SHL_INT:
    case Instruction::SHR_INT:
    case Instruction::ADD_INT_2ADDR:
    case Instruction::SUB_INT_2ADDR:
    case Instruction::MUL_INT_2ADDR:
    case Instruction::DIV_INT_2ADDR:
    case Instruction::REM_INT_2ADDR:
    case Instruction::AND_INT_2ADDR:
    case Instruction::OR_INT_2ADDR:
    case Instruction::XOR_INT_2ADDR:
    case Instruction::SHL_INT_2ADDR:
    case Instruction::SHR_INT_2ADDR:
    case Instruction::USHR_INT_2ADDR:
    case Instruction::ADD_INT_LIT16:
    case Instruction::RSUB_INT:
    case Instruction::MUL_INT_LIT16:
    case Instruction::DIV_INT_LIT16:
    case Instruction::REM_INT_LIT16:
    case Instruction::AND_INT_LIT16:
    case Instruction::OR_INT_LIT16:
    case Instruction::XOR_INT_LIT16:
    case Instruction::ADD_INT_LIT8:
    case Instruction::RSUB_INT_LIT8:
    case Instruction::MUL_INT_LIT8:
    case Instruction::DIV_INT_LIT8:
    case Instruction::REM_INT_LIT8:
    case Instruction::AND_INT_LIT8:
    case Instruction::OR_INT_LIT8:
    case Instruction::XOR_INT_LIT8:
    case Instruction::SHL_INT_LIT8:
    case Instruction::SHR_INT_LIT8:
    case Instruction::USHR_INT_LIT8:
      exp_type = ExpType_Int;
      break;
    case Instruction::NEG_LONG:
    case Instruction::NOT_LONG:
    case Instruction::INT_TO_LONG:
    case Instruction::FLOAT_TO_LONG:
    case Instruction::DOUBLE_TO_LONG:
    case Instruction::USHR_INT:
    case Instruction::ADD_LONG:
    case Instruction::SUB_LONG:
    case Instruction::MUL_LONG:
    case Instruction::DIV_LONG:
    case Instruction::REM_LONG:
    case Instruction::AND_LONG:
    case Instruction::OR_LONG:
    case Instruction::XOR_LONG:
    case Instruction::SHL_LONG:
    case Instruction::SHR_LONG:
    case Instruction::USHR_LONG:
    case Instruction::ADD_LONG_2ADDR:
    case Instruction::SUB_LONG_2ADDR:
    case Instruction::MUL_LONG_2ADDR:
    case Instruction::DIV_LONG_2ADDR:
    case Instruction::REM_LONG_2ADDR:
    case Instruction::AND_LONG_2ADDR:
    case Instruction::OR_LONG_2ADDR:
    case Instruction::XOR_LONG_2ADDR:
    case Instruction::SHL_LONG_2ADDR:
    case Instruction::SHR_LONG_2ADDR:
    case Instruction::USHR_LONG_2ADDR:
      exp_type = ExpType_Long;
      break;
    case Instruction::NEG_FLOAT:
    case Instruction::INT_TO_FLOAT:
    case Instruction::LONG_TO_FLOAT:
    case Instruction::DOUBLE_TO_FLOAT:
    case Instruction::ADD_FLOAT:
    case Instruction::SUB_FLOAT:
    case Instruction::MUL_FLOAT:
    case Instruction::DIV_FLOAT:
    case Instruction::REM_FLOAT:
    case Instruction::ADD_FLOAT_2ADDR:
    case Instruction::SUB_FLOAT_2ADDR:
    case Instruction::MUL_FLOAT_2ADDR:
    case Instruction::DIV_FLOAT_2ADDR:
    case Instruction::REM_FLOAT_2ADDR:
      exp_type = ExpType_Float;
      break;
    case Instruction::NEG_DOUBLE:
    case Instruction::INT_TO_DOUBLE:
    case Instruction::LONG_TO_DOUBLE:
    case Instruction::FLOAT_TO_DOUBLE:
    case Instruction::ADD_DOUBLE:
    case Instruction::SUB_DOUBLE:
    case Instruction::MUL_DOUBLE:
    case Instruction::DIV_DOUBLE:
    case Instruction::REM_DOUBLE:
    case Instruction::ADD_DOUBLE_2ADDR:
    case Instruction::SUB_DOUBLE_2ADDR:
    case Instruction::MUL_DOUBLE_2ADDR:
    case Instruction::DIV_DOUBLE_2ADDR:
    case Instruction::REM_DOUBLE_2ADDR:
      exp_type = ExpType_Double;
      break;
    default:
      exp_type = ExpType_Invalid;
      break;
  }

  return exp_type;
}

}  // namespace art
