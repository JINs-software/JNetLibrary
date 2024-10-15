#pragma once

#include <iostream>
#include <functional>
#include <memory>

// �ް��� �ϴ� Job �Լ��� ���� Ÿ�� ��ü�� ���ڸ� �޴� �͵� �ƴϰ�, ��ȯ�� �ִ� �͵� �ƴϴ�.
using CallbackType = std::function<void()>;

class Job
{
public:
	Job(CallbackType&& callback)
		: m_Callback(std::move(callback))
	{}

	// �� �����ڿ� ��ġ ����
	template<typename T, typename Ret, typename... Args>
	Job(std::shared_ptr<T> obj, Ret(T::* method)(Args...), Args&&... args)
	{
		m_Callback = [obj, method, args...]()
			{
				(obj.get()->*method)(args...);
			};
	}

	// ��������� std::shared_ptr�� �޴� ������
	template <typename T>
	Job(std::shared_ptr<T> obj) {
		std::cout << "Job ������ ȣ��!" << std::endl;
	}

	void Execute() {
		m_Callback();
	}

private:
	CallbackType m_Callback;
};

