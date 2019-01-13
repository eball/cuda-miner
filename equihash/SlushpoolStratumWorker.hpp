#pragma	once

#include "core/StratumWorker.hpp"
#include "core/difficulty.h"

class SlushpoolStratumWorker : public core::StratumWorker
{
public:
	SlushpoolStratumWorker(const std::string &aApiKey);

	void PostSolution(core::Solution::Ref aSolution) const override;

protected:
	bool OnCreateTransport() override;
	bool OnConnected() override;

protected:
	std::string	_apiKey;

public:
	beam::Difficulty	_powDiff;

};

