/*
 * Copyright (C) 2014 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
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

#include "constant_propagation.h"

namespace art {

void ConstantPropagation::Run() {
  // Process nodes (basic blocks) in post-order in the dominator tree.
  for (HPostOrderIterator it(*graph_); !it.Done(); it.Advance()) {
    HBasicBlock* block = it.Current();
    // Traverse this block's instructions in (forward) order and
    // replace the ones that can be statically evaluated by a
    // compile-time counterpart.
    for (HInstructionIterator it(block->GetInstructions());
         !it.Done(); it.Advance()) {
      HInstruction* inst = it.Current();

      /* Constant folding: replace `c <- a op b' with a compile-time
         evaluation of `a op b' if `a' and `b' are constant.  */
      if (inst != nullptr && inst->IsAdd()) {
        HAdd* add = inst->AsAdd();
        HConstant* constant = add->TryStaticEvaluation();
        if (constant != nullptr) {
          // Replace `inst` with a compile-time constant.
          inst->GetBlock()->InsertInstructionBefore(constant, inst);
          inst->ReplaceWith(constant);
          inst->GetBlock()->RemoveInstruction(inst);
        }
      }

      // Constant condition.
      /* TODO: Also process `HCondition' nodes separately from `HIf'
         nodes, e.g. to replace a constant condition used as an
         expression?  */
      if (inst != nullptr && inst->IsIf()) {
        HIf* if_inst = inst->AsIf();
        HInstruction* cond_inst = if_inst->InputAt(0);
        DCHECK(cond_inst->IsCondition());
        HCondition* condition = cond_inst->AsCondition();
        if (condition->GetLeft()->IsIntConstant()
            && condition->GetRight()->IsIntConstant()) {
          // Install a `HGoto' node at the end of the block that will
          // eventually replace the `HIf' node.
          HGoto* goto_inst = new(graph_->GetArena()) HGoto();
          HBasicBlock* current_block = if_inst->GetBlock();
          DCHECK(current_block->GetLastInstruction() == if_inst);
          current_block->AddInstruction(goto_inst);

          // Evaluate the condition and remove the link between the
          // current block and the unreached successor.
          int32_t lhs_val = condition->GetLeft()->AsIntConstant()->GetValue();
          int32_t rhs_val = condition->GetRight()->AsIntConstant()->GetValue();
          bool cond;
          switch (condition->GetCondition()) {
            case kCondEQ: cond = lhs_val == rhs_val; break;
            case kCondNE: cond = lhs_val != rhs_val; break;
            case kCondLT: cond = lhs_val <  rhs_val; break;
            case kCondLE: cond = lhs_val <= rhs_val; break;
            case kCondGT: cond = lhs_val >  rhs_val; break;
            case kCondGE: cond = lhs_val >= rhs_val; break;
            default:
              cond = true;
              LOG(FATAL) << "Unreachable";
          }
          HBasicBlock* unreached_block =
            cond ? if_inst->IfFalseSuccessor() : if_inst->IfTrueSuccessor();
          unreached_block->RemovePredecessor(current_block);
          current_block->RemoveSuccessor(unreached_block);

          // Remove instruction(s) from current block.
          bool condition_needs_materialization =
            condition->NeedsMaterialization();
          current_block->RemoveInstruction(if_inst);
          /* Remove the condition if it does not need materialization.
             We could also leave this task a dead code elimination
             pass.  */
          if (!condition_needs_materialization)
            current_block->RemoveInstruction(condition);
        }
      }
    }
  }
}

}  // namespace art
