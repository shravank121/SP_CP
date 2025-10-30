🐚 MyShell — Custom Linux Shell

MyShell is a lightweight custom command-line shell built in C. It supports command execution, piping, redirection, background processes, and command history — providing a minimal yet powerful shell experience.

🚀 Features

Execute system commands (ls, pwd, echo, etc.)

Input and output redirection (<, >, >>)

Piping between commands (|)

Run commands in the background (&)

Command chaining with ;

Command history

Figlet banner at startup

Error handling for invalid commands

🗂️ Project Structure

SP_CP/
├── CP_1.c # Main shell code
├── dockerfile # Docker setup file
├── test_commands.sh # Automated test script
├── test_report.txt # Test execution log
├── tests/ # Test output folder (ignored in Git)
├── .myshell_history # Command history file
├── .gitignore
└── README.md

⚙️ Setup and Run

Clone the repository
git clone https://github.com/shravank121/SP_CP.git

cd SP_CP

Build the shell
gcc CP_1.c -o myshell

Run MyShell
./myshell

You’ll see:

\ \ / /| | ___ ___ _ __ ___ ___ | |_ ___
\ \ /\ / / _ \ |/ / _ | ' ` _ \ / _ \ | __/ _
\ V V / __/ | (| () | | | | | | __/ | || () |
_/_/ _||__/|| || ||_| ___/

💻 Example Commands

myshell:/app> pwd
myshell:/app> echo Hello World
myshell:/app> ls | grep .c
myshell:/app> cat < input.txt > output.txt
myshell:/app> sleep 5 &
myshell:/app> cd /tmp; pwd
myshell:/app> history
myshell:/app> exit

🧪 Testing

Run automated tests:
bash test_commands.sh

Results are logged in:
test_report.txt

Each test checks commands, redirection, and pipes automatically.

🐳 Run with Docker (Optional)

Build the image:
docker build -t myshell_latest .

Run the container:
docker run -it myshell_latest

Then start the shell inside:
./myshell

⚠️ Known Limitations

Arrow key navigation is not yet supported

Background job tracking is limited

cd may have issues with nested relative paths

👨‍💻 Author

Shravan Kulkarni
GitHub: https://github.com/shravank121