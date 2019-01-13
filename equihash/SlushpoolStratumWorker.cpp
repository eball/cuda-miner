#include "pch.hpp"
#include "base/BigInteger.hpp"
#include "base/Endian.hpp"
#include "base/Convertors.hpp"
#include "base/Logging.hpp"
#include "SlushpoolStratumWorker.hpp"
#include "BeamWork.hpp"
#include "core/Statistics.hpp"
#include "core/TcpAsyncTransport.hpp"
#include "base/Utils.hpp"

class AuthorizeCall : public core::StratumWorker::Call
{
public:
    AuthorizeCall(core::StratumWorker &aWorker, const std::string &aApiKey) : _worker(aWorker), _apiKey(aApiKey)
    {
		_id = std::to_string(_worker.CreateCallId());
		_name = "mining.authorize";
    }

	void Serialize(std::string &aBuffer) const override
	{
        std::stringstream json;
        json << "{\"method\":\"mining.authorize\", \"params\":[\"" << _apiKey << "\",\"\"], \"id\":" << _id << ",\"jsonrpc\":\"2.0\"}" << std::endl;
        aBuffer = json.str();
    }

    void OnTimeout() override
	{
	}

	bool HasResult() const override
	{
		return true;
	}

    bool OnResult(const JSonVar &aResult) override
	{
        if (auto error = aResult[core::StratumWorker::kError]) {
			if (error->IsNull()) {
				if (auto result = aResult[core::StratumWorker::kResult]) {
                    return result->GetBoolValue();
                }
            }
        }

        return false;
    }

protected:
	core::StratumWorker	&_worker;
	std::string			_apiKey;
};


class SubscribeCall : public core::StratumWorker::Call
{
public:
    SubscribeCall(core::StratumWorker &aWorker, const std::string &aApiKey) : _worker(aWorker), _apiKey(aApiKey)
    {
		_id = std::to_string(_worker.CreateCallId());
		_name = "mining.subscribe";
    }

	void OnTimeout() override
	{
	}

	bool HasResult() const override
	{
		return true;
	}

    bool OnResult(const JSonVar &aResult) override
	{
        if (auto error = aResult[core::StratumWorker::kError]) {
			if (error->IsNull()) {
				if (auto result = aResult[core::StratumWorker::kResult]) {
                    if (result->IsArray() && result->GetArrayLen() > 2){
                        std::string poolNonceStr = (*result)[1]->GetValue();
                        LOG(Info) << "get nonce prefix: " << poolNonceStr;
                        _worker.SetPoolNonce(poolNonceStr);
                    }
                }

                _worker.RemoteCall(new AuthorizeCall(_worker, _apiKey));
                return true;
            }
        }

        return false;
    }

	void Serialize(std::string &aBuffer) const override
	{
        std::stringstream json;
		// TODO: reconnecting with the same session id
		json << "{\"id\": " << _id << ", \"method\": \"mining.subscribe\", \"params\": [\"opencl-miner\"],\"jsonrpc\":\"2.0\"}" << std::endl;

        aBuffer = json.str();
	}

protected:
	core::StratumWorker	&_worker;
	std::string			_apiKey;
};

class SubmitCall : public core::StratumWorker::Call
{
public:
	SubmitCall(core::StratumWorker &aWorker, const std::string &aApiKey, core::Solution::Ref aSolution) 
        : _worker(aWorker), _solution(aSolution), _apiKey(aApiKey)
	{
		_id = std::to_string(_worker.CreateCallId());
		_name = "mining.submit";
	}

	bool OnResult(const JSonVar &aResult) override
	{
		// {"id": 4, "result": true, "error": null}\n
        bool res = false;
        std::string reason;

        if (auto error = aResult[core::StratumWorker::kError]){
            if (!error->IsNull()){
                reason = error->GetValue();
            }else{
                res = true;
            }
        }
		if (auto result = aResult[core::StratumWorker::kResult]) {
            if (!result->IsNull()){
                res = result->GetBoolValue();
            }
		}

        if (res) {
            _solution->OnAccepted((unsigned)std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now() - _start).count());
        }
        else {
            _solution->OnRejected((unsigned)std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now() - _start).count(), reason);
        }
		return true;
	}

	void OnTimeout() override
	{
	}

	bool HasResult() const override
	{
		return true;
	}

	void Serialize(std::string &aBuffer) const override
	{
		std::string nonce, solStr("");
		_solution->PrintNonce(nonce, false);
		_solution->PrintSolution(solStr);


    	std::stringstream json;
		json << "{\"params\": [\"" << _apiKey << "\", \"" << _solution->GetWorkId()  <<"\"," 
			<< "\"" << solStr << "\", \"0000\", \"" << nonce << "\"], \"id\": " << _id << ", "
			<< "\"method\": \"mining.submit\",\"jsonrpc\":\"2.0\"}" << std::endl;

        aBuffer = json.str();
		LOG(Info) << "Submitting solution for job #" << _id << " with nonce " << nonce;
	}

protected:
	core::StratumWorker		&_worker;
	core::Solution::Ref		_solution;
	core::Performance::Time	_start = std::chrono::system_clock::now();
	std::string			_apiKey;
};

class DifficultyNotify : public core::StratumWorker::Call
{
public:
	DifficultyNotify(core::StratumWorker &aWorker) : _worker(aWorker)
	{
		_name = "mining.set_difficulty";
	}

	bool OnCall(const JSonVar &aParams) override
	{
        if (aParams.IsArray() && aParams.GetArrayLen() > 0){
            auto difficulty = aParams[(unsigned int)0];
            static_cast<SlushpoolStratumWorker&>(_worker)._powDiff = beam::Difficulty(difficulty->GetULongValue());
            LOG(Info) << "set difficulty to: " << static_cast<SlushpoolStratumWorker&>(_worker)._powDiff.ToFloat();
        }

		return true;
	}

	bool HasParams() const override
	{
		return true;
	}

protected:
	core::StratumWorker	&_worker;
};


class PoolJobNotify : public core::StratumWorker::Call
{
public:
	PoolJobNotify(core::StratumWorker &aWorker) : _worker(aWorker)
	{
		_name = "mining.notify";
	}

	bool OnCall(const JSonVar &aParams) override
	{
            LOG(Debug) << "new work";
        if (aParams.IsArray() && aParams.GetArrayLen() >= 9){
            LOG(Debug) << "parse params";
            if (BeamWork::Ref work = new BeamWork()) {
                auto id = aParams[(unsigned int)0];
                work->_id = id->GetValue();
        LOG(Debug) << "get work id" << work->_id;
                if (auto input = aParams[2]) {
                    work->_input.Import(input->GetValue(), true);
                    work->_powDiff = static_cast<SlushpoolStratumWorker&>(_worker)._powDiff;
                    work->_poolNonce = _worker._poolNonce;

                    if(auto clean = aParams[8]){
                        if (clean->GetBoolValue()){
                            BeamWork::sNonce = GenRandomU64();
                        }
                    }

                    LOG(Info) << "New job #" << work->_id << " at difficulty " << work->_powDiff.ToFloat();
                    _worker.SetWork(*work);
                    return true;
                }
            }
        }
		return true;
	}

	bool HasParams() const override
	{
		return true;
	}

protected:
	core::StratumWorker	&_worker;
};


SlushpoolStratumWorker::SlushpoolStratumWorker(const std::string &aApiKey) : _apiKey(aApiKey)
{
	Register(new PoolJobNotify(*this));
	Register(new DifficultyNotify(*this));
}

bool SlushpoolStratumWorker::OnCreateTransport()
{
	_transport = new core::TcpAsyncTransport(_server, atoi(_port.c_str()));

	return _transport;
}

bool SlushpoolStratumWorker::OnConnected()
{
	_connectedCounter++;
	RemoteCall(new SubscribeCall(*this, _apiKey));
	return true;
}

void SlushpoolStratumWorker::PostSolution(core::Solution::Ref aSolution) const
{
	const_cast<SlushpoolStratumWorker*>(this)->RemoteCall(new SubmitCall(*const_cast<SlushpoolStratumWorker*>(this), _apiKey, aSolution));
}

