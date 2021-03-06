#pragma	once

#include "core/Work.hpp"
#include "core/difficulty.h"

class BeamWork : public core::Work
{
public:
	typedef Reference<BeamWork> Ref;

public:
	BeamWork();
	BeamWork(const std::string &aBlockHeader, const std::string &aNonce);

	unsigned char * GetNonce() override;
	uint32_t GetNonceSize() const override;

	core::Work::Ref CreateForThread(uint32_t aThreadId) const override;
	void Increment(unsigned aCount) override;

	virtual unsigned char * GetData();
	virtual const unsigned char * GetData() const;
	virtual uint32_t GetDataSize() const;

public:
	static std::atomic<uint64_t> sNonce;

public:
	beam::Difficulty	_powDiff;
	uint64_t			_nonce;
	BigInteger			_input;
	std::vector<uint8_t> _poolNonce;

};
