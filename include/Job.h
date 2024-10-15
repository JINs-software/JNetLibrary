#pragma once

#include <iostream>
#include <functional>
#include <memory>

// 받고자 하는 Job 함수의 람다 타입 자체가 인자를 받는 것도 아니고, 반환이 있는 것도 아니다.
using CallbackType = std::function<void()>;

class Job
{
public:
	Job(CallbackType&& callback)
		: m_Callback(std::move(callback))
	{}

	// 이 생성자와 일치 예상
	template<typename T, typename Ret, typename... Args>
	Job(std::shared_ptr<T> obj, Ret(T::* method)(Args...), Args&&... args)
	{
		m_Callback = [obj, method, args...]()
			{
				(obj.get()->*method)(args...);
			};
	}

	// 명시적으로 std::shared_ptr을 받는 생성자
	template <typename T>
	Job(std::shared_ptr<T> obj) {
		std::cout << "Job 생성자 호출!" << std::endl;
	}

	void Execute() {
		m_Callback();
	}

private:
	CallbackType m_Callback;
};

