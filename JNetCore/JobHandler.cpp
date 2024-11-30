#include "JobHandler.h"

void JobHandler::executablePush(JobRef job)
{
	// Push
	//const auto ret = InterlockedIncrement(&m_AtomicJobCnt);
	//m_AllocatedJobs.Enqueue(job);
	//if (ret > 1)	return;
	// => JobCnt�� ���������� Job Enqueue �� �ٸ� �����尡 Dequeue�� �õ��ϸ�,
	//	  Dequeue�� �����Ͽ� ��� �����嵵 Job�� �������� ���ϴ� ���� �߻� ����

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