#!/usr/bin/env bash

set -uo pipefail

SCRIPT_DIR="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(cd -- "${SCRIPT_DIR}/.." && pwd)"
BUILD_DIR="${BUILD_DIR:-${PROJECT_ROOT}/build}"
DATA_TIMEOUT_SECONDS="${DATA_TIMEOUT_SECONDS:-60}"
LOG_DIR="${BUILD_DIR}/tests/all_data_logs"
SUMMARY_LOG="${LOG_DIR}/summary.log"

declare -A EXPECTED_EXIT_CODES=(
    ["data/sample/sample.data"]="94"
    ["data/testcases/array_test1.data"]="123"
    ["data/testcases/array_test2.data"]="43"
    ["data/testcases/basicopt1.data"]="88"
    ["data/testcases/bulgarian.data"]="159"
    ["data/testcases/expr.data"]="58"
    ["data/testcases/gcd.data"]="178"
    ["data/testcases/hanoi.data"]="20"
    ["data/testcases/lvalue2.data"]="175"
    ["data/testcases/magic.data"]="106"
    ["data/testcases/manyarguments.data"]="40"
    ["data/testcases/multiarray.data"]="115"
    ["data/testcases/naive.data"]="94"
    ["data/testcases/pi.data"]="137"
    ["data/testcases/qsort.data"]="105"
    ["data/testcases/queens.data"]="171"
    ["data/testcases/statement_test.data"]="50"
    ["data/testcases/superloop.data"]="134"
    ["data/testcases/tak.data"]="186"
)

run_unit_tests=true
run_data_tests=true

usage() {
    cat <<'EOF'
Usage: tests/run_all_tests.sh [--unit-only | --data-only] [--help]

Environment variables:
  BUILD_DIR             CMake build directory (default: <project>/build)
  DATA_TIMEOUT_SECONDS  Timeout for each .data case (default: 60)
EOF
}

for argument in "$@"; do
    case "${argument}" in
        --unit-only)
            run_data_tests=false
            ;;
        --data-only)
            run_unit_tests=false
            ;;
        --help|-h)
            usage
            exit 0
            ;;
        *)
            echo "unknown argument: ${argument}" >&2
            usage >&2
            exit 2
            ;;
    esac
done

mkdir -p "${LOG_DIR}"
: > "${SUMMARY_LOG}"

log() {
    printf '%s\n' "$*" | tee -a "${SUMMARY_LOG}"
}

log "project: ${PROJECT_ROOT}"
log "build:   ${BUILD_DIR}"
log "logs:    ${LOG_DIR}"

if ! cmake \
    -S "${PROJECT_ROOT}" \
    -B "${BUILD_DIR}" \
    -DCMAKE_BUILD_TYPE=Debug \
    -DBUILD_TESTING=ON \
    2>&1 | tee "${LOG_DIR}/configure.log"; then
    log "[FAIL] CMake configuration"
    exit 1
fi

if ! cmake --build "${BUILD_DIR}" -j2 \
    2>&1 | tee "${LOG_DIR}/build.log"; then
    log "[FAIL] build"
    exit 1
fi

overall_failed=0

if [[ "${run_unit_tests}" == true ]]; then
    log ""
    log "========== CTest =========="
    if ctest \
        --test-dir "${BUILD_DIR}" \
        --output-on-failure \
        2>&1 | tee "${LOG_DIR}/ctest.log"; then
        log "[PASS] all configured CTest tests"
    else
        log "[FAIL] one or more configured CTest tests"
        overall_failed=1
    fi
fi

if [[ "${run_data_tests}" == true ]]; then
    log ""
    log "========== CPU data tests =========="

    simulator="${PROJECT_ROOT}/code"
    if [[ ! -x "${simulator}" ]]; then
        log "[FAIL] simulator executable not found: ${simulator}"
        exit 1
    fi

    mapfile -d '' data_files < <(
        find "${PROJECT_ROOT}/data" -type f -name '*.data' -print0 \
            | sort -z
    )

    data_passed=0
    data_failed=0

    for data_file in "${data_files[@]}"; do
        relative_path="${data_file#"${PROJECT_ROOT}/"}"
        case_name="${relative_path#data/}"
        case_name="${case_name%.data}"
        case_log_name="${case_name//\//__}"
        stdout_log="${LOG_DIR}/${case_log_name}.stdout.log"
        stderr_log="${LOG_DIR}/${case_log_name}.stderr.log"

        if [[ -z "${EXPECTED_EXIT_CODES[${relative_path}]+known}" ]]; then
            log "[FAIL] ${relative_path}: expected exit code is not configured"
            ((data_failed += 1))
            overall_failed=1
            continue
        fi

        expected="${EXPECTED_EXIT_CODES[${relative_path}]}"
        timeout "${DATA_TIMEOUT_SECONDS}" \
            "${simulator}" \
            < "${data_file}" \
            > "${stdout_log}" \
            2> "${stderr_log}"
        command_status=$?

        if [[ ${command_status} -eq 124 ]]; then
            log "[FAIL] ${relative_path}: timeout after ${DATA_TIMEOUT_SECONDS}s"
            log "       stderr: ${stderr_log}"
            ((data_failed += 1))
            overall_failed=1
            continue
        fi

        if [[ ${command_status} -ne 0 ]]; then
            log "[FAIL] ${relative_path}: simulator exited with ${command_status}"
            log "       stderr: ${stderr_log}"
            ((data_failed += 1))
            overall_failed=1
            continue
        fi

        actual="$(awk 'NF { print $1; exit }' "${stdout_log}")"
        if [[ "${actual}" == "${expected}" ]]; then
            log "[PASS] ${relative_path}: ${actual}"
            ((data_passed += 1))
        else
            log "[FAIL] ${relative_path}: expected ${expected}, actual ${actual:-<empty>}"
            log "       stdout: ${stdout_log}"
            log "       stderr: ${stderr_log}"
            ((data_failed += 1))
            overall_failed=1
        fi
    done

    for configured_path in "${!EXPECTED_EXIT_CODES[@]}"; do
        if [[ ! -f "${PROJECT_ROOT}/${configured_path}" ]]; then
            log "[FAIL] configured data file is missing: ${configured_path}"
            ((data_failed += 1))
            overall_failed=1
        fi
    done

    log "CPU data tests: ${data_passed} passed, ${data_failed} failed"
fi

log ""
if [[ ${overall_failed} -eq 0 ]]; then
    log "[PASS] requested test suites completed successfully"
else
    log "[FAIL] requested test suites contain failures"
fi

exit "${overall_failed}"
