/*
 * Copyright 2015 WebAssembly Community Group participants
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

//
// Misc optimizations that are useful for and/or are only valid for
// emscripten output.
//

#include <wasm.h>
#include <pass.h>

namespace wasm {

struct PostEmscripten : public WalkerPass<WasmWalker<PostEmscripten>> {
  // When we have a Load from a local value (typically a GetLocal) plus a constant offset,
  // we may be able to fold it in.
  // The semantics of the Add are to wrap, while wasm offset semantics purposefully do
  // not wrap. So this is not always safe to do. For example, a load may depend on
  // wrapping via
  //      (2^32 - 10) + 100   =>  wrap and load from address 90
  // Without wrapping, we get something too large, and an error. *However*, for
  // asm2wasm output coming from Emscripten, we allocate the lowest 1024 for mapped
  // globals. Mapped globals are simple types (i32, float or double), always
  // accessed directly by a single constant. Therefore if we see (..) + K where
  // K is less then 1024, then if it wraps, it wraps into [0, 1024) which is at best
  // a mapped global, but it can't be because they are accessed directly (at worst,
  // it's 0 or an unused section of memory that was reserved for mapped globlas).
  // Thus it is ok to optimize such small constants into Load offsets.
  void visitLoad(Load *curr) {
    if (curr->offset) return;
    auto add = curr->ptr->dyn_cast<Binary>();
    if (!add || add->op != Add) return;
    assert(add->type == i32);
    auto c = add->right->dyn_cast<Const>();
    if (!c) {
      c = add->left->dyn_cast<Const>();
      if (c) {
        // if one is a const, it's ok to swap
        add->left = add->right;
        add->right = c;
      }
    }
    if (!c) return;
    auto value = c->value.geti32();
    if (value >= 0 && value < 1024) {
      // foldable, by the above logic
      curr->ptr = add->left;
      curr->offset = value;
    }
  }
};

static RegisterPass<PostEmscripten> registerPass("post-emscripten", "miscellaneous optimizations for Emscripten-generated code");

} // namespace wasm
