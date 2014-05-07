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
 * distributed under the License Is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#ifndef ART_COMPILER_DEX_EXPRESSION_H_
#define ART_COMPILER_DEX_EXPRESSION_H_

#include <string>
#include <vector>
#include <map>

#include "compiler_ir.h"
#include "utils/allocation.h"

namespace art {

// Forward declarations.
class VirtualRegister;
class VirtualRegisterMappingComparator;

/**
 * @brief Defines kind of expression depending on number of operands
 * and semantics of operation.
 * @details There is a 1:N mapping between this an ExpressionKind.
 */
enum GeneralExpressionKind {
  GenExpKind_NoExp,
  GenExpKind_UnaryExp,
  GenExpKind_BinaryExp,
};

/**
 * @brief Defines kind of expression depending on operation.
 */
enum ExpressionKind {
  ExpKind_Invalid = 0,
  ExpKind_ConstSet,
  ExpKind_Add,
  ExpKind_Sub,
  ExpKind_Mul,
  ExpKind_Phi,
  ExpKind_Cast,
  ExpKind_Div,
  ExpKind_Rem,
  ExpKind_And,
  ExpKind_Or,
  ExpKind_Xor,
  ExpKind_Shl,
  ExpKind_Shr,
  ExpKind_Ushr,
};

/**
 * @brief Defines type of expression depending on primitive type
 * of result after operation is applied to operands.
 */
enum ExpressionType {
  ExpType_Invalid = 0,
  ExpType_Int,
  ExpType_Long,
  ExpType_Float,
  ExpType_Double,
};

/**
 * @brief Base virtual class for representing an expression.
 */
class Expression: public ArenaObject {
 public:
  explicit Expression() {
  }

  virtual ~Expression() {
  }

  void* operator new(size_t size, ArenaAllocator* allocator) {
    return allocator->Alloc(size, kArenaAllocExpression);
  }
  void operator delete(void* p) {}

  virtual std::string ToString(const CompilationUnit * c_unit) = 0;

  virtual void GetChildren(std::vector<Expression*>& children) const {
  }

  virtual bool IsBytecodeExpression() const {
    return false;
  }

  virtual bool IsConstant() const {
    return false;
  }

  virtual bool EvaluatesToConstant() const {
    return false;
  }

  virtual bool IsVirtualRegister() const {
    return false;
  }

  /**
   * @brief Converts a MIR to an expression.
   * @details Uses vr_to_expression in order to find expressions for the
   * operands of the MIR in order to create an expression tree.
   * @param vr_to_expression Map of virtual registers and corresponding
   * expression that was used to assign a value to it. Only expressions
   * that are unary or binary expressions can be in this list since others
   * do not assign to a VR.
   * @return Returns expression representing the MIR or null if
   * conversion Is not successful.
   */
  static Expression* MirToExpression(ArenaAllocator* arena, MIR* mir,
      std::map<VirtualRegister*, Expression*>* vr_to_expression = 0);

  /**
   * @brief Converts a list of MIRs to expressions.
   * @details Takes the MIRs in order and converts them to expression.
   * If during conversion of a MIR we find that we have already generated
   * an expression for another MIR that sets the current operand, we use
   * the other expression to create an expression tree.
   * @return Returns a map of each MIR to its corresponding expression.
   * If conversion was not successful, the map will contain a null
   * expression for that MIR.
   */
  static std::map<MIR*, Expression*> MirsToExpressions(ArenaAllocator* arena, const std::vector<MIR*>& list_of_mirs);
};

/**
 * @brief Expression used for representing wide and non-wide virtual registers.
 */
class VirtualRegister: public Expression {
 public:
  /**
   * @brief Non-wide virtual register constructor
   * @param ssaReg the ssa register representing the VR.
   */
  explicit VirtualRegister(int ssaReg)
      : Expression(), low_ssa_reg_(ssaReg), high_ssa_reg_(0), wide_(false) {
  }

  /**
   * @brief Wide virtual register constructor
   * @param low_ssa_reg the ssa register representing the low VR
   * @param high_ssa_reg the ssa register representing the high VR
   */
  VirtualRegister(int low_ssa_reg, int high_ssa_reg)
    : Expression(), low_ssa_reg_(low_ssa_reg), high_ssa_reg_(high_ssa_reg), wide_(true) {
    }

  /**
   * @brief Always returns true since class represents virtual register.
   */
  bool IsVirtualRegister() const {
    return true;
  }

  bool IsWide() const {
    return wide_;
  }

  int GetLowSSAReg() const {
    return low_ssa_reg_;
  }

  /**
   * @brief Get the highSSA value, if this is a wide VR
   * @return highSSA value if wide, else -1
   */
  int GetHighSSAReg() const {
    return (wide_ == true) ? high_ssa_reg_ : -1;
  }

  /**
   * @brief Converts one of the ssa registers(or two for wide case) to
   * representation of virtual register.
   * @details Once a virtual register Is created, we then look through the
   * vr_to_expression map and if we find a match of an expression for that VR,
   * we return that instead.
   * @param vr_to_expression Map of virtual registers to expressions used to
   * assign to them. Allowed to be null.
   * @param low_ssa_reg the ssa register representing the low VR
   * @param high_ssa_reg the ssa register representing the high VR
   * @param wide whether we are to construct a wide VR
   * @return Returns expression tree representing the operand. It Is either
   * a virtual register or an expression tree that was used to assign to it.
   */
  static Expression* ConvertToVR(ArenaAllocator* arena, std::map<VirtualRegister*, Expression*> * vr_to_expression,
      int low_ssa_reg, int high_ssa_reg = 0, bool wide = false);

  std::string ToString(const CompilationUnit * c_unit);

  /**
   * @brief The comparator must be able to access the fields of the
   * virtual register.
   */
  friend class VirtualRegisterMappingComparator;

 protected:
  int low_ssa_reg_;
  int high_ssa_reg_;
  bool wide_;

 private:
  /**
   * @brief Default Constructor
   * @details Disabled by making it private.
   */
  VirtualRegister();

  /**
   * @brief Helper method to look for an expression for self.
   * @param vr_to_expression Map of virtual registers to expression trees.
   * @return Returns found expression. If nothing was found, returns 0.
   */
  Expression* FindExpressionForVR(std::map<VirtualRegister*, Expression*> * vr_to_expression);
};

/**
 * @brief Helper class for being able to compare VirtualRegister instances
 * for equality.
 */
class VirtualRegisterMappingComparator: std::unary_function<VirtualRegister*, bool> {
 public:
  /**
   * @brief Constructor.
   * @param vR Virtual register we want to compare against.
   */
  explicit VirtualRegisterMappingComparator(VirtualRegister* vR) {
    holder_ = vR;
  }

  /**
   * @brief Functor used for checking for equality among two VRs.
   * @param vr_to_expression_mapping Pair of virtual register to expression. Only
   * key Is used for comparison.
   * @return Returns true if key from pair matches the virtual register
   * we are looking for.
   */
  bool operator()(const std::pair<VirtualRegister*, Expression*> vr_to_expression_mapping);

 private:
  /**
   * @brief Used to keep track of the virtual register we are comparing other
   * instances against in order to find a match.
   */
  VirtualRegister* holder_;
};

/**
 * @brief Virtual class used to represent dalvik bytecodes.
 * @details There is a 1:1 mapping between a Dalvik bytecode and a
 * BytecodeExpression
 */
class BytecodeExpression: public Expression {
 public:
  BytecodeExpression(VirtualRegister* assign_to, ExpressionKind kind, ExpressionType type, MIR* insn)
      : Expression(), assignment_to_(assign_to), mir_(insn), exp_kind_(kind), exp_type_(type) {
    DCHECK(assign_to != nullptr);
    DCHECK(insn != nullptr);
  }

  /**
   * @brief Always returns true since this is a bytecode expression.
   */
  bool IsBytecodeExpression() const {
    return true;
  }

  virtual VirtualRegister* GetAssignmentTo() const {
    return assignment_to_;
  }

  /**
   * @brief Get the associated MIR
   * @return The MIR associated with the Expression.
   */
  virtual MIR* GetMir() const {
    return mir_;
  }

  /**
   * @brief Returns expression kind.
   */
  virtual ExpressionKind GetExpressionKind() const {
    return exp_kind_;
  }

  /**
   * @brief Returns expression type depending on type of result
   * and not on the operands.
   */
  virtual ExpressionType GetExpressionType() const {
    return exp_type_;
  }

  /**
   * @brief Used to create a MIR when given parameters that can build
   * expression. This can only be used for float and int versions.
   * @param exp_kind Expression kind
   * @param exp_type Expression type
   * @param assign_toVR Dalvik register we are assigning To(not ssa)
   * @param lhsVR Dalvik register for lhs operand
   * @param rhsVR Dalvik register for rhs operand
   * @return MIR that was created. If no MIR was created, returns 0.
   */
  static MIR* CreateMir(ArenaAllocator* arena, ExpressionKind exp_kind, ExpressionType exp_type, int assign_toVR,
      int lhsVR, int rhsVR = 0);

 protected:
  /**
   * @brief Keeps track of virtual register we are assigning To.
   * @details Dalvik bytecodes always have a virtual register that the result
   * Is assigned To. In order to simplify dealing with expression tree, we
   * keep the assignment to as part of the b
   */
  VirtualRegister* assignment_to_;

  /**
   * @brief Associated MIR.
   */
  MIR* mir_;

  /**
   * @brief Keeps track of expression kind of this expression.
   */
  ExpressionKind exp_kind_;

  /**
   * @brief Defines the type of the result of the operation. Namely, this defines
   * type of assignment_to field in how it Is intended to be interpreted.
   */
  ExpressionType exp_type_;

  /**
   * @brief Returns general expression kind(noexp, unexp, or binexp) for
   * the dalvik opcode.
   * @param dalvikInstruction::Code dalvik opcode
   */
  static GeneralExpressionKind GetGenExpressionKind(ArenaAllocator* arena, Instruction::Code dalvikOpcode);

  /**
   * @brief Returns expression kind for the dalvik opcode.
   * @param dalvikInstruction::Code dalvik opcode
   */
  static ExpressionKind GetExpressionKind(ArenaAllocator* arena, Instruction::Code dalvikOpcode);

  /**
   * @brief Returns expression type for the dalvik opcode.
   * @param dalvikInstruction::Code dalvik opcode
   */
  static ExpressionType GetExpressionType(Instruction::Code dalvikOpcode);

 private:
  /**
   * @brief Default Constructor
   * @details Disabled by making it private.
   */
  BytecodeExpression();
};

/**
 * @brief Used to represent a bytecode expression which has two operands.
 * @details Used with bytecodes of form "binop vAA, vBB, vCC",
 * "binop/2addr vA, vB", "binop/lit16 vA, vB, #+CCCC", and
 * "binop/lit8 vAA, vBB, #+CC"
 */
class BinaryExpression: public BytecodeExpression {
 public:
  BinaryExpression(VirtualRegister* assign_to, Expression* lhs, Expression* rhs, ExpressionKind kind,
                   ExpressionType type, MIR* mir)
      : BytecodeExpression(assign_to, kind, type, mir), lhs_(lhs), rhs_(rhs) {
    DCHECK(lhs_ != nullptr);
    DCHECK(rhs_ != nullptr);
  }

  Expression* GetLhs() const {
    return lhs_;
  }

  Expression* GetRhs() const {
    return rhs_;
  }

  bool EvaluatesToConstant() const {
    return lhs_->EvaluatesToConstant() && rhs_->EvaluatesToConstant();
  }

  virtual void GetChildren(std::vector<Expression*>& children) const;

  std::string ToString(const CompilationUnit * c_unit);

  static BinaryExpression* MirToExpression(ArenaAllocator* arena, MIR* mir,
      std::map<VirtualRegister*, Expression*> * vr_to_expression,
      ExpressionKind exp_kind);

  static BinaryExpression* NewExpression(ArenaAllocator* arena, MIR* mir, VirtualRegister* assign_to,
      Expression* lhs, Expression* rhs, ExpressionKind exp_kind,
      ExpressionType exp_type);

 protected:
  Expression* lhs_;
  Expression* rhs_;

 private:
  /**
   * @brief Default Constructor
   * @details Disabled by making it private.
   */
  BinaryExpression();
};

/**
 * @brief Used with bytecodes of form "unop vA, vB", const, and move
 */
class UnaryExpression: public BytecodeExpression {
 public:
  UnaryExpression(VirtualRegister* assign_to, Expression* operand,
                  ExpressionKind exp_kind, ExpressionType exp_type, MIR* mir)
      : BytecodeExpression(assign_to, exp_kind, exp_type, mir), operand_(operand) {
    DCHECK(operand_ != nullptr);
  }

  bool EvaluatesToConstant() const {
    return operand_->EvaluatesToConstant();
  }

  virtual void GetChildren(std::vector<Expression*>& children) const;

  std::string ToString(const CompilationUnit * c_unit);

  /**
   * @brief Creates an instance of UnaryExpression in arena space.
   * @param mir MIR associated with expression. Cannot be null.
   * @param assign_to Virtual register that expression assigns To.
   * @return Returns Newly created expression or 0 when one cannot
   * be created from the given arguments.
   */
  static UnaryExpression* NewExpression(ArenaAllocator* arena, MIR* mir, VirtualRegister* assign_to,
                                        Expression* operand, ExpressionKind exp_kind, ExpressionType exp_type);

  /**
   * @brief Converts a MIR to expression representation.
   * @param vr_to_expression Map of virtual registers and corresponding
   * expression that was used to assign a value to it. Note that this
   * Is updated by function when New expression is successfully created.
   * @return Newly created expression. If failed to create, returns 0.
   */
  static UnaryExpression* MirToExpression(ArenaAllocator* arena, MIR* mir,
                                          std::map<VirtualRegister*, Expression*> * vr_to_expression,
                                          ExpressionKind exp_kind);

 protected:
  /**
   * @brief Used to keep track of the expression tree of operand.
   */
  Expression* operand_;

 private:
  /**
   * @brief Default Constructor
   * @details Disabled by making it private.
   */
  UnaryExpression();
};

/**
 * @brief Expression used to represent a constant.
 */
class ConstantExpression: public Expression {
 public:
  explicit ConstantExpression(int64_t value, bool is_wide = false)
      : value_(value), wide_(is_wide) {
  }

  explicit ConstantExpression(float constant)
      : wide_(false) {
    // We need to break strict aliasing rules so we quiet down compiler
    // by using char* first.
    char *ptr = reinterpret_cast<char *>(&constant);

    value_ = static_cast<int64_t>(*reinterpret_cast<int32_t*>(ptr));
  }

  explicit ConstantExpression(double constant)
      : wide_(true) {
    // We need to break strict aliasing rules so we quiet down compiler
    // by using char* first.
    char *ptr = reinterpret_cast<char *>(&constant);

    value_ = *reinterpret_cast<int64_t*>(ptr);
  }

  bool IsConstant() const {
    return true;
  }

  bool EvaluatesToConstant() const {
    return true;
  }

  bool IsWide() const {
    return wide_;
  }

  /**
   * @brief Returns constant value into desired primitive type.
   * @tparam Desired type of constant value
   */
  template<typename desired_type>
  desired_type GetValue() {
    // We need to break strict aliasing rules so we quiet down compiler
    // by using char* first.
    char *ptr = reinterpret_cast<char *>(&value_);

    // Since the backing store Is 64-bit integer, we need to
    // reinterpret to desired type.
    return *reinterpret_cast<desired_type *>(ptr);
  }

  /**
   * @brief Converts constant to string representation.
   * @details Since it Is not known how the value will be interpreted,
   * everything Gets printed as a 64-bit integer.
   * @param c_unit the compilation unit.
   * @return string representation.
   */
  std::string ToString(const CompilationUnit * c_unit);

  /**
   * @brief Used to create a ConstantExpression using arena.
   * @param value The constant value.
   * @param wide Whether we are creating a wide(64-bit) constant.
   * @return Returns the constant expression instance created.
   */
  static ConstantExpression* NewExpression(ArenaAllocator* arena, int64_t value, bool wide = false);

 private:
  /**
   * @brief Default Constructor
   * @details Disabled by making it private.
   */
  ConstantExpression();

  int64_t value_;
  bool wide_;
};

}  // namespace art

#endif  // ART_COMPILER_DEX_EXPRESSION_H_
