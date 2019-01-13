#include "pch.hpp"
#include "BeamWork.hpp"
#include "base/Endian.hpp"
#include "base/Convertors.hpp"
#include "base/Utils.hpp"

std::atomic<uint64_t> BeamWork::sNonce(GenRandomU64());

// 0007fff800000000000000000000000000000000000000000000000000000000 is stratum diff 32
// 003fffc000000000000000000000000000000000000000000000000000000000 is stratum diff 4
// 00ffff0000000000000000000000000000000000000000000000000000000000 is stratum diff 1
static double TargetToDiff(uint32_t* target)
{
	unsigned char* tgt = (unsigned char*)target;
	uint64_t m =
		(uint64_t)tgt[30] << 24 |
		(uint64_t)tgt[29] << 16 |
		(uint64_t)tgt[28] << 8 |
		(uint64_t)tgt[27] << 0;

	if (!m) {
		return 0.;
	}
	else {
		return (double)0xffff0000UL / m;
	}
}

BeamWork::BeamWork()
{
}

BeamWork::BeamWork(const std::string &aBlockHeader, const std::string &aNonce) : _input(32, true)
{
	_input.Import(aBlockHeader, true);
	HexToBin(aNonce, (unsigned char*)&_nonce, 8);
}

unsigned char * BeamWork::GetNonce()
{
	return (unsigned char *)&_nonce;
}

uint32_t BeamWork::GetNonceSize() const
{
	return 8;
}

core::Work::Ref BeamWork::CreateForThread(uint32_t aThreadId) const
{
	if (BeamWork::Ref work = new BeamWork(*this)) {
		work->_nonce = sNonce++;
		
		uint8_t* noncePoint = (uint8_t*) &work->_nonce;

		uint32_t poolNonceBytes = std::min<uint32_t>(_poolNonce.size(), 6); 	// Need some range left for miner
		work->_nonce = (work->_nonce << 8*poolNonceBytes);

		for (uint32_t i=0; i<poolNonceBytes; i++) {			// Prefix pool nonce
			noncePoint[i] = _poolNonce[i];
		}

		return work.get();
	}
	return core::Work::Ref();
}

void BeamWork::Increment(unsigned aCount)
{
	_nonce = sNonce++;
	uint8_t* noncePoint = (uint8_t*) &_nonce;

	uint32_t poolNonceBytes = std::min<uint32_t>(_poolNonce.size(), 6); 	// Need some range left for miner
	_nonce = (_nonce << 8*poolNonceBytes);

	for (uint32_t i=0; i<poolNonceBytes; i++) {			// Prefix pool nonce
		noncePoint[i] = _poolNonce[i];
	}

}

unsigned char * BeamWork::GetData()
{
	return _input.GetBytes();
}

const unsigned char * BeamWork::GetData() const
{
	return _input.GetBytes();
}

uint32_t BeamWork::GetDataSize() const
{
	return _input.GetBytesLength();
}

