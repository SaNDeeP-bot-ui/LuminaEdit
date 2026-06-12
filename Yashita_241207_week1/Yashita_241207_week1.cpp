#include <ctype.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <termios.h>
#include <unistd.h>

struct termios originalTerminalSettings;

void die(const char *message) {
    perror(message);
    exit(1);
}

void disableRawMode() {
    if (tcsetattr(STDIN_FILENO, TCSAFLUSH, &originalTerminalSettings) == -1) {
        die("tcsetattr");
    }
}

void enableRawMode() {
    if (tcgetattr(STDIN_FILENO, &originalTerminalSettings) == -1) {
        die("tcgetattr");
    }

    atexit(disableRawMode);

    struct termios raw = originalTerminalSettings;

    /*
        Wrong / incomplete idea:
        If we only disable ECHO, the terminal stops printing typed characters,
        but it still waits for Enter before sending input to the program.

            raw.c_lflag &= ~(ECHO);

        Correction:
        Disable ICANON too, so input is read key-by-key instead of line-by-line.
    */
    raw.c_lflag &= ~(ECHO | ICANON);

    /*
        ISIG:
        Normally Ctrl-C and Ctrl-Z are handled by the terminal.
        Ctrl-C sends SIGINT and usually kills the program.
        Ctrl-Z sends SIGTSTP and suspends it.

        In raw mode, the editor should receive these as keypresses,
        so ISIG is disabled.
    */
    raw.c_lflag &= ~(ISIG);

    /*
        IEXTEN:
        Disables extra terminal processing such as Ctrl-V behavior.
        This gives the editor more direct control over input.
    */
    raw.c_lflag &= ~(IEXTEN);

    /*
        IXON:
        Normally Ctrl-S pauses terminal output and Ctrl-Q resumes it.
        A text editor commonly uses Ctrl-S for save and Ctrl-Q for quit,
        so the terminal should not steal those keypresses.
    */
    raw.c_iflag &= ~(IXON);

    /*
        ICRNL:
        Normally the terminal converts carriage return '\r' into newline '\n'
        when Enter is pressed. In raw mode, we want the real byte.
    */
    raw.c_iflag &= ~(ICRNL);

    /*
        BRKINT, INPCK, ISTRIP:
        These are older terminal / serial input processing flags.
        They are disabled to make input as raw and direct as possible.
    */
    raw.c_iflag &= ~(BRKINT | INPCK | ISTRIP);

    /*
        OPOST:
        Normally the terminal post-processes output.
        For example, it may convert '\n' into "\r\n".

        In raw mode we disable this, so we manually print "\r\n"
        whenever we want a proper new line.
    */
    raw.c_oflag &= ~(OPOST);

    /*
        CS8:
        Sets character size to 8 bits.
        This helps ensure full 8-bit input characters are received.
    */
    raw.c_cflag |= (CS8);

    /*
        VMIN and VTIME control read() behavior.

        VMIN = 0:
        read() can return even if no key is pressed.

        VTIME = 1:
        read() waits at most 0.1 seconds before returning.
    */
    raw.c_cc[VMIN] = 0;
    raw.c_cc[VTIME] = 1;

    if (tcsetattr(STDIN_FILENO, TCSAFLUSH, &raw) == -1) {
        die("tcsetattr");
    }
}

int main() {
    enableRawMode();

    while (1) {
        char c = '\0';

        int bytesRead = read(STDIN_FILENO, &c, 1);

        if (bytesRead == -1 && errno != EAGAIN) {
            die("read");
        }

        if (bytesRead == 0) {
            continue;
        }

        /*
            Ctrl-Q has ASCII value 17.
            We use Ctrl-Q to quit the program.
        */
        if (c == 17) {
            break;
        }

        /*
            Print every keypress.

            Control characters are printed only as ASCII values.
            Normal characters are printed as both ASCII value and character.
        */
        if (iscntrl(c)) {
            printf("%d\r\n", c);
        } else {
            printf("%d ('%c')\r\n", c, c);
        }
    }

    return 0;
}
