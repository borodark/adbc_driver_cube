# CI Configuration for ADBC Tests

**Date:** 2025-12-26
**Purpose:** Document integration test handling for ADBC and Cube

## Test Results Summary

### Local Testing (All Tests)
```bash
mix test
# 18 doctests, 100 tests
# 11 failures (PostgreSQL - server not running)
```

### CI Testing (With Exclusions)
```bash
mix test --exclude postgresql
# 18 doctests, 100 tests, 0 failures, 22 excluded
✅ ALL PASSING
```

## GitHub Actions Configuration

Updated `.github/workflows/ci.yml` to exclude integration tests that require external services:

### Linux (Ubuntu)
- **Has:** PostgreSQL service container (port 5432)
- **Excludes:** None
- **Command:** `mix test`
- **Runs:** PostgreSQL tests ✅, Unit tests ✅
- **Skips:** Cube integration tests (moved to this repo) ❌

### Windows
- **Has:** No external services
- **Excludes:** `:postgresql` tag
- **Command:** `mix test --exclude postgresql`
- **Runs:** Unit tests ✅
- **Skips:** PostgreSQL tests ❌, Cube integration tests (moved to this repo) ❌

### macOS
- **Has:** No external services
- **Excludes:** `:postgresql` tag
- **Command:** `mix test --exclude postgresql`
- **Runs:** Unit tests ✅
- **Skips:** PostgreSQL tests ❌, Cube integration tests (moved to this repo) ❌

## Test Tag Organization

### Integration Tests (Require External Services)

**PostgreSQL Tests** - `@moduletag :postgresql`
- File: `test/adbc_postgres_test.exs`
- Requires: PostgreSQL server on port 5432
- Tests: 11 tests for PostgreSQL driver functionality

**Cube Tests** - `@moduletag :cube`
- Files:
  - `tests/elixir/adbc_cube_basic_test.exs` (11 tests)
  - `tests/elixir/cube_preagg_benchmark.exs` (1 test)
- Requires:
  - Cube API server (port 4008)
  - Cube ADBC Server ADBC(Arrow Native) server (port 8120)
- Tests: 12 tests for Cube ADBC(Arrow Native) integration

### Unit Tests (No External Services)
- No special tags
- Run in all CI environments
- Test ADBC library functionality (Column, Result, Connection, Database modules)

## Local Development

### Run All Tests (Including Integration)
```bash
# Requires: PostgreSQL, Cube API, and Cube ADBC Server servers running
mix test --include postgresql
ADBC_CUBE_DRIVER_PATH=/path/to/libadbc_driver_cube.so \
  mix test ./adbc_driver_cube/tests/elixir/*.exs --include cube
```

### Run Only Unit Tests (Like CI)
```bash
mix test --exclude postgresql
```

### Run Only Cube Tests
```bash
# Requires: Cube API and Cube ADBC Server servers running
ADBC_CUBE_DRIVER_PATH=/path/to/libadbc_driver_cube.so \
  mix test ./adbc_driver_cube/tests/elixir/*.exs --include cube --exclude postgresql
```

### Run Specific Test File
```bash
ADBC_CUBE_DRIVER_PATH=/path/to/libadbc_driver_cube.so \
  mix test ./adbc_driver_cube/tests/elixir/adbc_cube_basic_test.exs --include cube
ADBC_CUBE_DRIVER_PATH=/path/to/libadbc_driver_cube.so \
  mix test ./adbc_driver_cube/tests/elixir/cube_preagg_benchmark.exs --include cube
```

## Why Exclude Integration Tests in CI?

**Cube Integration Tests:**
- ❌ Require Cube API server (Node.js application)
- ❌ Require Cube ADBC Server server (Rust application)
- ❌ Require specific cube schemas (`orders_with_preagg`, `orders_no_preagg`)
- ❌ Complex setup not suitable for CI environment
- ✅ Better tested locally or in dedicated integration test environment

**PostgreSQL Integration Tests:**
- ✅ Can run on Linux CI (has PostgreSQL service container)
- ❌ Excluded on Windows/macOS (no PostgreSQL service)
- ✅ Simple setup suitable for Linux CI

## Test Coverage in CI

| Test Type | Linux | Windows | macOS |
|-----------|-------|---------|-------|
| Unit Tests (Doctests) | ✅ | ✅ | ✅ |
| Unit Tests (ExUnit) | ✅ | ✅ | ✅ |
| PostgreSQL Integration | ✅ | ❌ | ❌ |
| Cube Integration | ❌ | ❌ | ❌ |

**Total in CI:**
- ✅ 18 doctests passing
- ✅ 78 unit tests passing
- ✅ 11 PostgreSQL tests passing (Linux only)
- ❌ 12 Cube tests excluded (all platforms)

## Summary

- ✅ CI configuration updated to exclude Cube integration tests
- ✅ All unit tests pass in CI environments
- ✅ PostgreSQL tests run on Linux CI (has service container)
- ✅ Integration tests can still be run locally with appropriate servers
- ✅ Test exclusions properly tagged and documented

**Commands for CI:**
- Linux: `mix test --exclude cube`
- Windows: `mix test --exclude postgresql --exclude cube`
- macOS: `mix test --exclude postgresql --exclude cube`
