#pragma once
#include <functional>
#include <thread>
#include <queue>
#include <mutex>
#include <atomic>
#include <condition_variable>
#include <cstdint>
#include <string>
#include <vector>
#include "DirectXMath.h"

using namespace DirectX;



class JobSystem
{
public:
	JobSystem() = default;
	~JobSystem()
	{
		Shutdown();
	}

	JobSystem(const JobSystem&) = delete;
	JobSystem& operator=(const JobSystem&) = delete;
	JobSystem(JobSystem&&) = delete;
	JobSystem& operator=(JobSystem&&) = delete;

	bool Initialize(int workerCount)
	{
		if (workerCount <= 0)
			return false;

		Shutdown();
		{
			std::lock_guard<std::mutex> lock(queueMutex);
			stop = false;
			activeJobs.store(0);
		}

		try
		{
			for (int i = 0; i < workerCount; ++i)
			{
				workers.emplace_back([this] {
					WorkerLoop();
				});
			}
		}
		catch (...)
		{
			Shutdown();
			return false;
		}

		return true;
	}

	bool SubmitJob(const std::function<void()>& job)
	{
		{
			std::lock_guard<std::mutex> lock(queueMutex);
			if (stop)
				return false;

			jobs.push(job);
		}
		condition.notify_one();
		return true;
	}

	void SubmitMainThread(std::function<void()> job)
	{
		std::lock_guard<std::mutex> lock(mainThreadMutex);
		mainThreadJobs.push(std::move(job));
	}

	void ExecuteMainThreadJobs()
	{
		std::queue<std::function<void()>> localJobs;

		{
			std::lock_guard<std::mutex> lock(mainThreadMutex);
			std::swap(localJobs, mainThreadJobs);
		}

		while (!localJobs.empty())
		{
			localJobs.front()();
			localJobs.pop();
		}
	}

	void Wait()
	{
		std::unique_lock<std::mutex> lock(queueMutex);
		finishedCondition.wait(lock, [this] {
			return jobs.empty() && activeJobs.load() == 0;
		});
	}

	void Shutdown()
	{
		{
			std::lock_guard<std::mutex> lock(queueMutex);
			stop = true;
		}

		condition.notify_all();
		for (std::thread& worker : workers)
		{
			if (worker.joinable())
				worker.join();
		}
		workers.clear();
	}



private:

	void WorkerLoop()
	{
		while (true)
		{
			std::function<void()> job;
			{
				std::unique_lock<std::mutex> lock(queueMutex);
				condition.wait(lock, [this] { return stop || !jobs.empty(); });

				if (stop && jobs.empty())
					return;

				job = std::move(jobs.front());
				jobs.pop();

				activeJobs++;
			}
			job();

			{
				std::lock_guard<std::mutex> lock(queueMutex);
				activeJobs--;
				if (jobs.empty() && activeJobs.load() == 0)
					finishedCondition.notify_all();
			}
		}
	}

	std::vector<std::thread> workers;
	std::queue<std::function<void()>> jobs;

	std::mutex queueMutex;
	std::condition_variable condition;

	// A default-constructed or shut-down queue must reject work. Otherwise a
	// job submitted before Initialize() would remain queued with no worker able
	// to execute it, and Wait() would block forever.
	bool stop = true;
	std::atomic<int> activeJobs = 0;

	std::queue<std::function<void()>> mainThreadJobs;
	std::mutex mainThreadMutex;
	std::condition_variable finishedCondition;
		
};

