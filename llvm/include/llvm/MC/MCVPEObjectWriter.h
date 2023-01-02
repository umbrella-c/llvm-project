//===- llvm/MC/MCVPEObjectWriter.h - VPE Object Writer -*- C++ -*-===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_MC_MCVPEOBJECTWRITER_H
#define LLVM_MC_MCVPEOBJECTWRITER_H

#include "llvm/MC/MCObjectWriter.h"
#include <memory>

namespace llvm {

class MCAsmBackend;
class MCContext;
class MCFixup;
class MCValue;
class raw_pwrite_stream;

  class MCVPEObjectTargetWriter : public MCObjectTargetWriter {
    virtual void anchor();

    const unsigned Machine;

  protected:
    MCVPEObjectTargetWriter(unsigned Machine_);

  public:
    virtual ~MCVPEObjectTargetWriter() = default;

    virtual Triple::ObjectFormatType getFormat() const override { return Triple::VPE; }
    static bool classof(const MCObjectTargetWriter *W) {
      return W->getFormat() == Triple::VPE;
    }

    unsigned getMachine() const { return Machine; }
    virtual unsigned getRelocType(MCContext &Ctx, const MCValue &Target,
                                  const MCFixup &Fixup, bool IsCrossSection,
                                  const MCAsmBackend &MAB) const = 0;
    virtual bool recordRelocation(const MCFixup &) const { return true; }
  };

  /// Construct a new VPE writer instance.
  ///
  /// \param MOTW - The target specific VPE writer subclass.
  /// \param OS - The stream to write to.
  /// \returns The constructed object writer.
  std::unique_ptr<MCObjectWriter>
  createVPEObjectWriter(std::unique_ptr<MCVPEObjectTargetWriter> MOTW,
                        raw_pwrite_stream &OS);
} // end namespace llvm

#endif // LLVM_MC_MCVPEOBJECTWRITER_H
