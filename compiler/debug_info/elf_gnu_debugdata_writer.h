
static void XzCompress(const std::vector<uint8_t>* src, std::vector<uint8_t>* dst) {
  // Configure the compression library.
  CrcGenerateTable();
  Crc64GenerateTable();
  CLzma2EncProps lzma2Props;
  Lzma2EncProps_Init(&lzma2Props);
  lzma2Props.lzmaProps.level = 1;  // Fast compression.
  Lzma2EncProps_Normalize(&lzma2Props);
  CXzProps props;
  XzProps_Init(&props);
  props.lzma2Props = &lzma2Props;
  // Implement the required interface for communication (written in C so no virtual methods).
  struct XzCallbacks : public ISeqInStream, public ISeqOutStream, public ICompressProgress {
    static SRes ReadImpl(void* p, void* buf, size_t* size) {
      auto* ctx = static_cast<XzCallbacks*>(reinterpret_cast<ISeqInStream*>(p));
      *size = std::min(*size, ctx->src_->size() - ctx->src_pos_);
      memcpy(buf, ctx->src_->data() + ctx->src_pos_, *size);
      ctx->src_pos_ += *size;
      return SZ_OK;
    }
    static size_t WriteImpl(void* p, const void* buf, size_t size) {
      auto* ctx = static_cast<XzCallbacks*>(reinterpret_cast<ISeqOutStream*>(p));
      const uint8_t* buffer = reinterpret_cast<const uint8_t*>(buf);
      ctx->dst_->insert(ctx->dst_->end(), buffer, buffer + size);
      return size;
    }
    static SRes ProgressImpl(void* , UInt64, UInt64) {
      return SZ_OK;
    }
    size_t src_pos_;
    const std::vector<uint8_t>* src_;
    std::vector<uint8_t>* dst_;
  };
  XzCallbacks callbacks;
  callbacks.Read = XzCallbacks::ReadImpl;
  callbacks.Write = XzCallbacks::WriteImpl;
  callbacks.Progress = XzCallbacks::ProgressImpl;
  callbacks.src_pos_ = 0;
  callbacks.src_ = src;
  callbacks.dst_ = dst;
  // Compress.
  SRes res = Xz_Encode(&callbacks, &callbacks, &props, &callbacks);
  CHECK_EQ(res, SZ_OK);
}

template <typename ElfTypes>
std::vector<uint8_t> MakeMiniDebugInfoInternal(
    InstructionSet isa,
    size_t rodata_section_size,
    size_t text_section_size,
    const ArrayRef<const MethodDebugInfo>& method_infos) {
  std::vector<uint8_t> buffer;
  buffer.reserve(KB);
  VectorOutputStream out("Mini-debug-info ELF file", &buffer);
  std::unique_ptr<ElfBuilder<ElfTypes>> builder(new ElfBuilder<ElfTypes>(isa, &out));
  builder->Start();
  // Mirror .rodata and .text as NOBITS sections.
  // It is needed to detected relocations after compression.
  builder->GetRoData()->WriteNoBitsSection(rodata_section_size);
  builder->GetText()->WriteNoBitsSection(text_section_size);
  WriteDebugSymbols(builder.get(), method_infos, false /* with_signature */);
  WriteCFISection(builder.get(), method_infos, DW_DEBUG_FRAME_FORMAT, false /* write_oat_paches */);
  builder->End();
  CHECK(builder->Good());
  std::vector<uint8_t> compressed_buffer;
  compressed_buffer.reserve(buffer.size() / 4);
  XzCompress(&buffer, &compressed_buffer);
  return compressed_buffer;
}
