#include "JobHandler.h"

void JobHandler::executablePush(JobRef job)
{
	m_AllocatedJobs.Enqueue(job);
	do {
		JobRef job;
		if (m_AllocatedJobs.Dequeue(job)) {
			job->Execute();
		}
		else break;
	} while (true);
}