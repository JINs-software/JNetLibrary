#include "JobHandler.h"

void JobHandler::executablePush(JobRef job)
{
	// Push
	//const auto ret = InterlockedIncrement(&m_AtomicJobCnt);
	//m_AllocatedJobs.Enqueue(job);
	//if (ret > 1)	return;
	// => JobCnt를 증가하지만 Job Enqueue 전 다른 스레드가 Dequeue를 시도하면,
	//	  Dequeue에 실패하여 어떠한 스레드도 Job을 수행하지 못하는 현상 발생 가능

	// Push
	m_AllocatedJobs.Enqueue(job);
	if (InterlockedIncrement(&m_AtomicJobCnt) > 1)	return;

	// Execute
	while (true) {
		JobRef job;
		if (!m_AllocatedJobs.Dequeue(job)) {
			DebugBreak();
		}

		job->Execute();
		if (InterlockedDecrement(&m_AtomicJobCnt) == 0) {
			return;
		}
	}
}