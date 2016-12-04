// Copyright (c) 2006, 2007 Julio M. Merino Vidal
// Copyright (c) 2008 Ilya Sokolov, Boris Schaeling
// Copyright (c) 2009 Boris Schaeling
// Copyright (c) 2010 Felipe Tanus, Boris Schaeling
// Copyright (c) 2011, 2012 Jeff Flinn, Boris Schaeling
// Copyright (c) 2016 Klemens D. Morgenstern
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)

/**
 * \file boost/process/system.hpp
 *
 * Defines a system function.
 */

#ifndef BOOST_PROCESS_SYSTEM_HPP
#define BOOST_PROCESS_SYSTEM_HPP

#include <boost/process/detail/config.hpp>
#include <boost/process/detail/on_exit.hpp>
#include <boost/process/child.hpp>
#include <boost/process/detail/async_handler.hpp>
#include <boost/process/detail/execute_impl.hpp>
#include <type_traits>
#include <mutex>
#include <condition_variable>

#if defined(BOOST_POSIX_API)
#include <boost/process/posix.hpp>
#endif

namespace boost {

namespace process {

namespace detail
{

template<typename IoService, typename ...Args>
inline int system_impl(
        std::true_type, /*needs ios*/
        std::true_type, /*has io_service*/
        Args && ...args)
{
    IoService & ios = ::boost::process::detail::get_io_service_var(args...);


    std::atomic_bool exited{false};

    child c(std::forward<Args>(args)...,
            ::boost::process::on_exit(
                [&](int exit_code, const std::error_code&)
                {
                    ios.post([&]{exited.store(true);});
                }));
    if (!c.valid())
        return -1;

    while (!exited.load())
        ios.poll();

    return c.exit_code();
}

template<typename IoService, typename ...Args>
inline int system_impl(
        std::true_type,  /*needs ios */
        std::false_type, /*has io_service*/
        Args && ...args)
{
    IoService ios;
    child c(ios, std::forward<Args>(args)...);
    if (!c.valid())
        return -1;

    ios.run();
    return c.exit_code();
}


template<typename IoService, typename ...Args>
inline int system_impl(
        std::false_type, /*needs ios*/
        std::true_type, /*has io_service*/
        Args && ...args)
{
    child c(std::forward<Args>(args)...);
    if (!c.valid())
        return -1;
    c.wait();
    return c.exit_code();
}

template<typename IoService, typename ...Args>
inline int system_impl(
        std::false_type, /*has async */
        std::false_type, /*has io_service*/
        Args && ...args)
{
    child c(std::forward<Args>(args)...
#if defined(BOOST_POSIX_API)
            ,::boost::process::posix::sig.dfl()
#endif
            );
    if (!c.valid())
        return -1;
    c.wait();
    return c.exit_code();
}

}

/** Launches a process and waits for its exit.
It works as std::system, though it allows
all the properties boost.process provides. It will execute the process and wait for it's exit; then return the exit_code.

\code{.cpp}
int ret = system("ls");
\endcode

\attention When used with Pipes it will almost always result in a dead-lock.

When using this function with an asynchronous properties and NOT passing an io_service object,
the system function will create one and run it. When the io_service is passed to the function,
the system function will check if it is active, and call the io_service::run function if not.

\par Coroutines

This function also allows to get a `boost::asio::yield_context` passed to use coroutines,
which will cause the stackful coroutine to yield and return when the process is finished.


\code{.cpp}
void cr(boost::asio::yield_context yield_)
{
    system("my-program", yield_);
}
\endcode

This will automatically suspend the coroutine until the program is finished.

*/
template<typename ...Args>
inline int system(Args && ...args)
{
    typedef typename ::boost::process::detail::needs_io_service<Args...>::type
            need_ios;
    typedef typename ::boost::process::detail::has_io_service<Args...>::type
            has_ios;
    return ::boost::process::detail::system_impl<boost::asio::io_service>(
            need_ios(), has_ios(),
            std::forward<Args>(args)...);
}


}}
#endif

