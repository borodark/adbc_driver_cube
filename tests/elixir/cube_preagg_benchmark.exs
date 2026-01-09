defmodule CubePreAggBenchmark do
  @moduledoc """
  Performance benchmark comparing queries with and without pre-aggregations.

  This benchmark measures the performance difference between:
  1. Queries WITHOUT pre-aggregations (goes through HTTP/JSON Cube API)
  2. Queries WITH pre-aggregations (goes through Arrow/FlatBuffers CubeStore direct)

  Run with:
    cd ~/projects/learn_erl/adbc
    mix test test/cube_preagg_benchmark.exs
  """

  use ExUnit.Case, async: false

  alias Adbc.{Database, Connection, Result}

  # Path to Cube ADBC driver
  @cube_driver_path System.get_env("ADBC_CUBE_DRIVER_PATH") ||
                      Path.join(:code.priv_dir(:adbc), "lib/libadbc_driver_cube.so")

  # Cube server connection details
  @cube_host "localhost"
  @cube_adbc_port 8120
  @cube_token "test"

  # Number of iterations for benchmarking
  @iterations 10
  @warmup_iterations 2

  setup_all do
    # Check if the Cube driver library exists
    unless File.exists?(@cube_driver_path) do
      raise """
      Cube driver not found at #{@cube_driver_path}.

      Set ADBC_CUBE_DRIVER_PATH or copy the driver into :adbc priv/lib.
      """
    end

    # Check if cubesqld is running
    case :gen_tcp.connect(String.to_charlist(@cube_host), @cube_adbc_port, [:binary], 1000) do
      {:ok, socket} ->
        :gen_tcp.close(socket)
        :ok

      {:error, :econnrefused} ->
        raise """
        Cube server (cubesqld) is not running on #{@cube_host}:#{@cube_adbc_port}.

        Start it with Arrow Native server:
          cd ~/projects/learn_erl/cube/examples/recipes/arrow-ipc
          ./start-cubesqld.sh
          # Or manually:
          export CUBESQL_CUBE_URL=http://localhost:4008/cubejs-api
          export CUBESQL_CUBE_TOKEN=test
          export CUBEJS_ARROW_PORT=4445
          export CUBESQL_ARROW_RESULTS_CACHE_ENABLED=true
          export CUBESQL_LOG_LEVEL=info
          ~/projects/learn_erl/cube/rust/cubesql/target/release/cubesqld
        """

      {:error, reason} ->
        raise "Failed to connect to Cube server: #{inspect(reason)}"
    end

    :ok
  end

  setup do
    # Start database with Cube driver
    db =
      start_supervised!(
        {Database,
         driver: @cube_driver_path,
         "adbc.cube.host": @cube_host,
         "adbc.cube.port": Integer.to_string(@cube_adbc_port),
         "adbc.cube.connection_mode": "native",
         "adbc.cube.token": @cube_token}
      )

    # Start connection
    conn = start_supervised!({Connection, database: db})

    %{db: db, conn: conn}
  end

  describe "Pre-aggregation Performance Benchmark" do
    test "compare query performance with and without pre-aggregations", %{conn: conn} do
      # Define the same query for both cubes
      # Query WITHOUT pre-agg uses Cube SQL (goes through HTTP/JSON)
      query_no_preagg = """
      SELECT
        orders_no_preagg.market_code,
        orders_no_preagg.brand_code,
        MEASURE(orders_no_preagg.count) as order_count,
        MEASURE(orders_no_preagg.total_amount_sum) as total_amount,
        MEASURE(orders_no_preagg.tax_amount_sum) as tax_amount,
        MEASURE(orders_no_preagg.subtotal_amount_sum) as subtotal_amount
      FROM
        orders_no_preagg
      WHERE
        orders_no_preagg.updated_at >= '2024-01-01'
      GROUP BY
        1, 2
      ORDER BY
        total_amount DESC
      LIMIT 100
      """

      # Query WITH pre-agg uses Cube SQL (MEASURE syntax)
      # Cube API will automatically use pre-aggregation
      query_with_preagg = """
      SELECT
        orders_with_preagg.market_code,
        orders_with_preagg.brand_code,
        MEASURE(orders_with_preagg.count) as order_count,
        MEASURE(orders_with_preagg.total_amount_sum) as total_amount,
        MEASURE(orders_with_preagg.tax_amount_sum) as tax_amount,
        MEASURE(orders_with_preagg.subtotal_amount_sum) as subtotal_amount
      FROM
        orders_with_preagg
      WHERE
        orders_with_preagg.updated_at >= '2024-01-01'
      GROUP BY
        1, 2
      ORDER BY
        total_amount DESC
      LIMIT 100
      """

      IO.puts("Both queries use MEASURE syntax and route through HybridTransport")
      IO.puts("Cube API will automatically use pre-aggregations for orders_with_preagg\n")

      IO.puts("\n" <> String.duplicate("=", 80))
      IO.puts("Pre-Aggregation Performance Benchmark")
      IO.puts(String.duplicate("=", 80))
      IO.puts("")

      # Warmup runs
      IO.puts("Warming up...")

      for _ <- 1..@warmup_iterations do
        {:ok, _} = Connection.query(conn, query_no_preagg)
        {:ok, _} = Connection.query(conn, query_with_preagg)
      end

      IO.puts("Warmup complete.\n")

      # Benchmark WITHOUT pre-aggregations (HTTP/JSON)
      IO.puts("Benchmarking WITHOUT pre-aggregations (HTTP/JSON to Cube API)...")
      times_no_preagg = benchmark_query(conn, query_no_preagg, @iterations)

      avg_no_preagg = Enum.sum(times_no_preagg) / length(times_no_preagg)
      min_no_preagg = Enum.min(times_no_preagg)
      max_no_preagg = Enum.max(times_no_preagg)

      IO.puts("Results:")
      IO.puts("  Average: #{format_time(avg_no_preagg)}ms")
      IO.puts("  Min: #{min_no_preagg}ms")
      IO.puts("  Max: #{max_no_preagg}ms")
      IO.puts("")

      # Benchmark WITH pre-aggregations (Arrow/FlatBuffers)
      IO.puts("Benchmarking WITH pre-aggregations (Arrow/FlatBuffers to CubeStore)...")
      times_with_preagg = benchmark_query(conn, query_with_preagg, @iterations)

      avg_with_preagg = Enum.sum(times_with_preagg) / length(times_with_preagg)
      min_with_preagg = Enum.min(times_with_preagg)
      max_with_preagg = Enum.max(times_with_preagg)

      IO.puts("Results:")
      IO.puts("  Average: #{format_time(avg_with_preagg)}ms")
      IO.puts("  Min: #{min_with_preagg}ms")
      IO.puts("  Max: #{max_with_preagg}ms")
      IO.puts("")

      # Calculate performance improvement
      speedup = avg_no_preagg / avg_with_preagg
      improvement_pct = (avg_no_preagg - avg_with_preagg) / avg_no_preagg * 100

      IO.puts(String.duplicate("=", 80))
      IO.puts("Performance Comparison")
      IO.puts(String.duplicate("=", 80))
      IO.puts("")
      IO.puts("Query: Aggregate orders by market and brand")
      IO.puts("Limit: 100 rows")
      IO.puts("Date filter: >= 2024-01-01")
      IO.puts("")
      IO.puts("WITHOUT Pre-Aggregation (HTTP/JSON):")
      IO.puts("  Average: #{format_time(avg_no_preagg)}ms")
      IO.puts("  Min: #{min_no_preagg}ms")
      IO.puts("  Max: #{max_no_preagg}ms")
      IO.puts("")
      IO.puts("WITH Pre-Aggregation (Arrow/FlatBuffers):")
      IO.puts("  Average: #{format_time(avg_with_preagg)}ms")
      IO.puts("  Min: #{min_with_preagg}ms")
      IO.puts("  Max: #{max_with_preagg}ms")
      IO.puts("")
      IO.puts("Performance Improvement:")
      IO.puts("  Speedup: #{Float.round(speedup, 2)}x faster")
      IO.puts("  Latency Reduction: #{Float.round(improvement_pct, 1)}%")
      IO.puts("")

      if speedup > 1.0 do
        IO.puts("✅ Pre-aggregation approach is #{Float.round(speedup, 2)}x FASTER!")
      else
        IO.puts("⚠️  Pre-aggregation approach is slower (#{Float.round(speedup, 2)}x)")
      end

      IO.puts("")
      IO.puts("Why Pre-Aggregation is Faster:")
      IO.puts("  • No JSON serialization/deserialization overhead")
      IO.puts("  • Direct binary protocol (FlatBuffers)")
      IO.puts("  • Columnar data format (Arrow)")
      IO.puts("  • No HTTP round-trip for data")
      IO.puts("  • Pre-computed aggregations reduce computation")
      IO.puts(String.duplicate("=", 80))
      IO.puts("")

      # Verify both queries return data
      {:ok, result_no_preagg} = Connection.query(conn, query_no_preagg)
      {:ok, result_with_preagg} = Connection.query(conn, query_with_preagg)

      materialized_no_preagg = Result.materialize(result_no_preagg)
      materialized_with_preagg = Result.materialize(result_with_preagg)

      # Both should have returned data
      assert length(materialized_no_preagg.data) > 0
      assert length(materialized_with_preagg.data) > 0

      # Show sample results
      IO.puts("Sample Results (first 5 rows):")
      IO.puts("WITHOUT Pre-Aggregation:")
      display_sample_results(materialized_no_preagg, 5)
      IO.puts("")
      IO.puts("WITH Pre-Aggregation:")
      display_sample_results(materialized_with_preagg, 5)
      IO.puts("")

      # Verify pre-aggregation is actually faster (or at least not significantly slower)
      # We allow some variance, but pre-agg should generally be faster
      assert speedup > 0.5, "Pre-aggregation approach should not be significantly slower"
    end
  end

  # Helper function to benchmark a query
  defp benchmark_query(conn, query, iterations) do
    Enum.map(1..iterations, fn i ->
      # Measure time from query start to result materialization
      start_time = System.monotonic_time(:millisecond)

      {:ok, result} = Connection.query(conn, query)
      _materialized = Result.materialize(result)

      end_time = System.monotonic_time(:millisecond)
      elapsed = end_time - start_time

      IO.write("  Iteration #{i}/#{iterations}: #{elapsed}ms\r")

      elapsed
    end)
    |> tap(fn _ -> IO.puts("") end)
  end

  # Helper function to display sample results
  defp display_sample_results(result, limit) do
    if length(result.data) == 0 do
      IO.puts("  (No data returned)")
    else
      do_display_results(result, limit)
    end
  end

  defp do_display_results(result, limit) do
    # Get column names
    column_names = Enum.map(result.data, & &1.name)

    # Get first N rows
    rows_count = result.data |> hd() |> Map.get(:data) |> length()
    rows_to_show = min(limit, rows_count)

    # Print header
    IO.puts("  " <> Enum.join(column_names, " | "))
    IO.puts("  " <> String.duplicate("-", Enum.join(column_names, " | ") |> String.length()))

    # Print rows
    for i <- 0..(rows_to_show - 1) do
      row_values =
        Enum.map(result.data, fn column ->
          value = Enum.at(column.data, i)
          format_value(value)
        end)

      IO.puts("  " <> Enum.join(row_values, " | "))
    end
  end

  defp format_value(nil), do: "NULL"
  defp format_value(value) when is_float(value), do: Float.round(value, 2) |> to_string()
  defp format_value(value), do: to_string(value)

  defp format_time(time) when is_float(time), do: Float.round(time, 2)
  defp format_time(time), do: time
end
