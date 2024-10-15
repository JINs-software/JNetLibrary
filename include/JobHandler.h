#pragma once
#include <LockFreeQueue.h>
#include "Job.h"

class JobHandler : public std::enable_shared_from_this<JobHandler>
{
	using JobRef = Job*;

public:
	void HandleJob_Async(CallbackType&& callback) {
		auto job = new Job(std::move(callback));
		executablePush(job);
	}

	template<typename T, typename Ret, typename... Args>
	void HandleJob_Async(Ret(T::* method)(Args...), Args... args)
	{
		std::shared_ptr<T> owner = std::static_pointer_cast<T>(shared_from_this());
		auto job = new Job(owner, method, std::forward<Args>(args)...);
		executablePush(job);
	}


private:
	void executablePush(JobRef job);

private:
	LockFreeQueue<JobRef> m_AllocatedJobs;
};

