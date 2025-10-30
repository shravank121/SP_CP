#!/bin/bash
# ==========================================================
# MyShell Extended Automated Test Suite (v2)
# ==========================================================

SHELL_BIN=./myshell
RESULT_DIR="test_results_v2"
REPORT_FILE="${RESULT_DIR}/test_report.txt"

mkdir -p "$RESULT_DIR"
echo "======== MyShell Extended Test Report ========" > "$REPORT_FILE"
echo "Run Time: $(date)" >> "$REPORT_FILE"
echo "===============================================" >> "$REPORT_FILE"

run_test() {
    local name="$1"
    local cmd="$2"
    local pattern="$3"
    local outfile="${RESULT_DIR}/${name}.out"

    echo "[$name]" >> "$REPORT_FILE"
    echo "Command(s): $cmd" >> "$REPORT_FILE"
    local start=$(date +%s%3N)

    echo "$cmd" | $SHELL_BIN > "$outfile" 2>&1
    local end=$(date +%s%3N)
    local elapsed=$((end - start))

    if grep -Eq "$pattern" "$outfile"; then
        echo "✅ PASS — Pattern matched: $pattern" >> "$REPORT_FILE"
    else
        echo "❌ FAIL — Pattern '$pattern' not found." >> "$REPORT_FILE"
        echo "First few lines:" >> "$REPORT_FILE"
        head -n 5 "$outfile" >> "$REPORT_FILE"
    fi

    echo "⏱️  Time: ${elapsed}ms" >> "$REPORT_FILE"
    echo "-----------------------------------------------" >> "$REPORT_FILE"
}

# ============ BASIC COMMANDS ============

run_test "Echo" "echo hello world" "hello world"
run_test "PWD" "pwd" "/app"
run_test "CD" "cd /; pwd" "^/$"

# ============ REDIRECTION ============

echo "redirect me" > redir_in.txt
run_test "Input_Redirection" "cat < redir_in.txt" "redirect me"
run_test "Output_Redirection" "echo test123 > out.txt; cat out.txt" "test123"
run_test "Append_Redirection" "echo line1 > file.txt; echo line2 >> file.txt; cat file.txt" "line1.*line2"

# ============ PIPES ============

echo -e "a\nb\napple" > pipe.txt
run_test "Single_Pipe" "cat pipe.txt | grep a" "a"
run_test "Multi_Pipe" "cat pipe.txt | grep a | wc -l" "[23]"
run_test "Pipe_With_Sort" "echo -e 'z\ny\nx' | sort" "x"

# ============ BACKGROUND PROCESSES ============

run_test "Background_Job" "sleep 1 &" "(&|PID|myshell)"

# ============ ERROR HANDLING ============

run_test "Invalid_Command" "wrongcmd" "(execvp|not found)"
run_test "Permission_Error" "cat /root/secretfile" "(Permission|denied)"

# ============ HISTORY AND BUILTINS ============

run_test "History" "ls; pwd; history" "history"
run_test "Chaining" "cd /tmp; pwd" "/tmp"

# ============ STRESS / EDGE ============

run_test "Long_Command" "$(printf 'echo x%.0s' {1..1000})" "x"
run_test "Multiple_Pipes_Long" "seq 1 100 | grep 5 | grep 0 | wc -l" "[1-9]"
run_test "Multiple_Redirections" "echo hi >a.txt; echo bye >>a.txt; cat a.txt" "hi.*bye"

echo "===============================================" >> "$REPORT_FILE"
echo "All extended tests completed." >> "$REPORT_FILE"

echo "Summary report in: $REPORT_FILE"
