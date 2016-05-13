#ifndef ART_RUNTIME_IMTABLE_H_
#define ART_RUNTIME_IMTABLE_H_

namespace art {

class ArtMethod;

class ImTable {
 public:
  static constexpr size_t kSize = IMT_SIZE;

  ArtMethod* Get(size_t index, size_t pointer_size) {
    DCHECK_LT(index, kSize);
    uint8_t* ptr = reinterpret_cast<uint8_t*>(this) + OffsetOfElement(index, pointer_size);
    if (pointer_size == 4) {
      uint32_t value = *reinterpret_cast<uint32_t*>(ptr);
      return reinterpret_cast<ArtMethod*>(value);
    } else {
      uint64_t value = *reinterpret_cast<uint64_t*>(ptr);
      return reinterpret_cast<ArtMethod*>(value);
    }
  }

  void Set(size_t index, ArtMethod* method, size_t pointer_size) {
    DCHECK_LT(index, kSize);
    uint8_t* ptr = reinterpret_cast<uint8_t*>(this) + OffsetOfElement(index, pointer_size);
    if (pointer_size == 4) {
      uintptr_t value = reinterpret_cast<uintptr_t>(method);
      DCHECK_EQ(static_cast<uint32_t>(value), value);  // Check that we dont lose any non 0 bits.
      *reinterpret_cast<uint32_t*>(ptr) = static_cast<uint32_t>(value);
    } else {
      *reinterpret_cast<uint64_t*>(ptr) = reinterpret_cast<uint64_t>(method);
    }
  }

  static size_t OffsetOfElement(size_t index, size_t pointer_size) {
    return index * pointer_size;
  }

  void Populate(ArtMethod** data, size_t pointer_size) {
    for (size_t i = 0; i < kSize; ++i) {
      Set(i, data[i], pointer_size);
    }
  }

  static size_t SizeInBytes(size_t pointer_size) {
    return kSize * pointer_size;
  }
};

}  // namespace art

#endif  // ART_RUNTIME_IMTABLE_H_

