#include "jni.h"

#include "mirror/class-inl.h"
#include "nth_caller_visitor.h"
#include "runtime.h"

namespace art {

// public static native void deoptimizeCurrentMethod();

extern "C" JNIEXPORT void JNICALL Java_Main_deoptimizeCurrentMethod(
    JNIEnv* env,
    jclass cls ATTRIBUTE_UNUSED) {
  ScopedObjectAccess soa(env);
  NthCallerVisitor caller(soa.Self(), 1, false);
  caller.WalkStack();
  CHECK(caller.caller != nullptr);
  caller.GetMethod()->Deoptimize();
}

// public static native void deoptimizeSpecialTestMethods();

extern "C" JNIEXPORT void JNICALL Java_Main_deoptimizeSpecialTestMethods(
    JNIEnv* env, jclass cls) {
  ScopedObjectAccess soa(env);
  StackHandleScope<1> hs(soa.Self());
  mirror::Class* c = soa.Decode<mirror::Class*>(cls);
  Handle<mirror::ClassLoader> loader(hs.NewHandle(c->GetClassLoader()));
  ClassLinker* class_linker = Runtime::Current()->GetClassLinker();
  mirror::Class* foo_cls = class_linker->FindClass(soa.Self(), "Foo", loader);
  std::vector<ArtMethod*> methods;
  auto pointer_size = class_linker->GetImagePointerSize();
  for (auto& method : foo_cls->GetMethods(pointer_size)) {
    if (strstr(method.GetName(), "specialTestMethod") != nullptr) {
      methods.push_back(&method);
    }
  }
  ArtMethod::Deoptimize(&methods);
}

}
