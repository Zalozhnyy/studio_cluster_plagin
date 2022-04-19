/*! \file
* \brief Класс таймера
*
* \author Заложный Н.В. niczz9797@gmail.com
*/


#ifndef CHRONOTIMER_H
#define CHRONOTIMER_H

#ifdef __cplusplus

#include <chrono>


class Timer
{
private:

	using clock_t = std::chrono::high_resolution_clock;
	using second_t = std::chrono::duration<double, std::ratio<1> >;

	std::chrono::time_point<clock_t> m_beg;
	std::chrono::time_point<clock_t> m_start;

public:
	Timer() : m_beg(clock_t::now()), m_start(clock_t::now())
	{
	}

	void reset()
	{
		m_beg = clock_t::now();
	}

	double elapsed() const
	{
		return std::chrono::duration_cast<second_t>(clock_t::now() - m_beg).count();
	}

	double execution_time() const
	{
		return std::chrono::duration_cast<second_t>(clock_t::now() - m_start).count();
	}

};
#endif

#ifdef __cplusplus
extern "C" {
#endif

	void initTimer();
	void resetTimer();
	double getTime();
	double getExecutiontime();

#ifdef __cplusplus
}
#endif

#endif // !CHRONOTIMER_H
