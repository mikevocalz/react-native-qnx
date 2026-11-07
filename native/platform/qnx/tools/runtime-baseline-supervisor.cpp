// Standalone POSIX application supervisor. Run as the application user. This
// owns one direct executable, not daemonized descendants or system services.
#ifndef _POSIX_C_SOURCE
#define _POSIX_C_SOURCE 200809L
#endif
#include <cerrno>
#include <chrono>
#include <csignal>
#include <cstdio>
#include <cstdlib>
#include <fcntl.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>

namespace {
volatile std::sig_atomic_t interrupted = 0;
void onSignal(int signal) { interrupted = signal; }
}

int main(int argc, char** argv) {
  if (argc < 5 || argv[3][0] != '-' || argv[3][1] != '-' || argv[3][2] != '\0') {
    std::fprintf(stderr, "usage: supervisor TIMEOUT_SECONDS LOG_PATH -- EXECUTABLE [ARG...]\n");
    return 2;
  }
  char* end = nullptr;
  errno = 0;
  const long seconds = std::strtol(argv[1], &end, 10);
  if (errno || !end || *end || seconds < 1 || seconds > 3600) {
    std::fprintf(stderr, "timeout must be an integer between 1 and 3600\n");
    return 2;
  }
  const int log = open(argv[2], O_WRONLY | O_CREAT | O_EXCL, 0600);
  if (log < 0) { std::perror("create application log"); return 2; }
  struct sigaction action {};
  action.sa_handler = onSignal;
  sigemptyset(&action.sa_mask);
  sigaction(SIGTERM, &action, nullptr);
  sigaction(SIGINT, &action, nullptr);
  sigaction(SIGHUP, &action, nullptr);
  const pid_t child = fork();
  if (child < 0) { std::perror("fork"); close(log); return 2; }
  if (child == 0) {
    signal(SIGTERM, SIG_DFL);
    signal(SIGINT, SIG_DFL);
    signal(SIGHUP, SIG_DFL);
    if (dup2(log, STDOUT_FILENO) < 0 || dup2(log, STDERR_FILENO) < 0) _exit(126);
    close(log);
    execvp(argv[4], argv + 4);
    std::perror("exec application");
    _exit(127);
  }
  close(log);
  using Clock = std::chrono::steady_clock;
  const auto deadline = Clock::now() + std::chrono::seconds(seconds);
  auto killDeadline = deadline;
  bool terminated = false;
  bool killed = false;
  bool timedOut = false;
  int status = 0;
  while (true) {
    const pid_t result = waitpid(child, &status, WNOHANG);
    if (result == child) break; // No signal is issued after this child is reaped.
    if (result < 0 && errno != EINTR) { std::perror("waitpid"); return 2; }
    const auto now = Clock::now();
    if (!terminated && (interrupted || now >= deadline)) {
      timedOut = !interrupted;
      kill(child, SIGTERM);
      terminated = true;
      killDeadline = now + std::chrono::seconds(1);
    } else if (terminated && !killed && now >= killDeadline) {
      kill(child, SIGKILL);
      killed = true;
    }
    const timespec pause{0, 10 * 1000 * 1000};
    nanosleep(&pause, nullptr);
  }
  const int result = timedOut ? 124 : interrupted ? 125 :
      WIFEXITED(status) ? WEXITSTATUS(status) : 128 + WTERMSIG(status);
  std::printf("RNQNX_BASELINE_CHILD_EXIT=%d\n", result);
  return result;
}
