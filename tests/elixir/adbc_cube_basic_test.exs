defmodule Adbc.CubeBasicTest do
  use ExUnit.Case, async: true

  alias Adbc.{Connection, Result, Column}

  @moduletag :cube
  @moduletag timeout: 30_000

  # Path to our custom-built Cube driver
  @cube_driver_path System.get_env("ADBC_CUBE_DRIVER_PATH") ||
                      Path.join(:code.priv_dir(:adbc), "lib/libadbc_driver_cube.so")

  # Cube server connection details
  @cube_host "localhost"
  @cube_adbc_port 8120
  @cube_token "test"

  setup_all do
    # Check if the Cube driver library exists
    unless File.exists?(@cube_driver_path) do
      raise """
      Cube driver not found at #{@cube_driver_path}.

      Set ADBC_CUBE_DRIVER_PATH or copy the driver into :adbc priv/lib.
      """
    end

    # Check if cubesqld is running on the Arrow Native port
    case :gen_tcp.connect(String.to_charlist(@cube_host), @cube_adbc_port, [:binary], 1000) do
      {:ok, socket} ->
        :gen_tcp.close(socket)
        :ok

      {:error, :econnrefused} ->
        raise """
        Cube server (cubesqld) is not running on #{@cube_host}:#{@cube_adbc_port}.

        Start it with:
          cd ~/projects/learn_erl/cube/examples/recipes/arrow-ipc
          ./start-cube-api.sh    # Terminal 1
          ./start-cubesqld.sh    # Terminal 2
        """

      {:error, reason} ->
        raise "Failed to connect to Cube server: #{inspect(reason)}"
    end

    :ok
  end

  setup do
    # Start database with custom Cube driver
    # Note: All options must use the "adbc.cube.*" prefix
    db =
      start_supervised!(
        {Adbc.Database,
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

  describe "basic connectivity" do
    test "runs simple SELECT 1 query", %{conn: conn} do
      assert {:ok, results} = Connection.query(conn, "SELECT 1 as test")

      materialized = Result.materialize(results)

      assert %Result{
               data: [
                 %Column{
                   name: "test",
                   type: :s64,
                   nullable: false,
                   data: [1]
                 }
               ]
             } = materialized
    end

    test "runs SELECT with different integer values", %{conn: conn} do
      assert {:ok, results} = Connection.query(conn, "SELECT 42 as answer")

      materialized = Result.materialize(results)

      assert %Result{
               data: [
                 %Column{
                   name: "answer",
                   type: :s64,
                   data: [42]
                 }
               ]
             } = materialized
    end
  end

  describe "data types" do
    test "handles STRING type", %{conn: conn} do
      assert {:ok, results} = Connection.query(conn, "SELECT 'hello world' as greeting")

      materialized = Result.materialize(results)

      assert %Result{
               data: [
                 %Column{
                   name: "greeting",
                   type: :string,
                   data: ["hello world"]
                 }
               ]
             } = materialized
    end

    test "handles DOUBLE/FLOAT type", %{conn: conn} do
      assert {:ok, results} = Connection.query(conn, "SELECT 3.14159 as pi")

      materialized = Result.materialize(results)

      assert %Result{
               data: [
                 %Column{
                   name: "pi",
                   type: type,
                   data: [pi_value]
                 }
               ]
             } = materialized

      # Type could be :f64 or :f32 depending on Arrow schema
      assert type in [:f64, :f32]
      assert is_float(pi_value)
      assert_in_delta pi_value, 3.14159, 0.00001
    end

    test "handles BOOLEAN type", %{conn: conn} do
      assert {:ok, results} = Connection.query(conn, "SELECT true as flag")

      materialized = Result.materialize(results)

      assert %Result{
               data: [
                 %Column{
                   name: "flag",
                   type: :boolean,
                   data: [true]
                 }
               ]
             } = materialized
    end
  end

  describe "Cube queries" do
    test "queries orders_with_preagg cube", %{conn: conn} do
      query = """
      SELECT
        market_code,
        brand_code,
        count
      FROM orders_with_preagg
      LIMIT 10
      """

      assert {:ok, results} = Connection.query(conn, query)
      materialized = Result.materialize(results)

      # Should have 3 columns
      assert length(materialized.data) == 3

      # Should have data
      first_column = hd(materialized.data)
      assert length(first_column.data) > 0
    end

    test "queries orders_with_preagg cube II", %{conn: conn} do
      query = """
      SELECT
      market_code,
      brand_code,
      MEASURE(count)
      FROM orders_with_preagg
      group by 1,2
      order by 3 desc
      LIMIT 8466
      """

      assert {:ok, results} = Connection.query(conn, query)
      materialized = Result.materialize(results)
      IO.inspect(materialized)
      # Should have 3 columns
      assert length(materialized.data) == 3

      # Should have data
      first_column = hd(materialized.data)
      assert length(first_column.data) > 0
    end

    test "queries orders_with_preagg cube III", %{conn: conn} do
      query = """
      SELECT
      market_code as market,
      brand_code as brand,
      MEASURE(count) as count_blin
      FROM orders_with_preagg
      group by 1,2
      order by 3 desc
      LIMIT 8466
      """

      assert {:ok, results} = Connection.query(conn, query)
      materialized = Result.materialize(results)
      IO.inspect(materialized)
      # Should have 3 columns
      assert length(materialized.data) == 3

      # Should have data
      first_column = hd(materialized.data)
      assert length(first_column.data) > 0
    end

    test "queries orders_with_preagg cube IV", %{conn: conn} do
      query = """
      SELECT
      market_code as market,
      brand_code as brand,
      MEASURE(count) as count_blin
      FROM orders_with_preagg
      WHERE (orders_with_preagg.market_code = 'BQ')
      group by 1,2
      HAVING (MEASURE(orders_with_preagg.count) > '1000')
      order by 3 desc
      """

      assert {:ok, results} = Connection.query(conn, query)
      materialized = Result.materialize(results)
      IO.inspect(materialized)
      # Should have 3 columns
      assert length(materialized.data) == 3

      # Should have data
      first_column = hd(materialized.data)
      assert length(first_column.data) > 0
    end

    test "queries orders_no_preagg cube", %{conn: conn} do
      query = """
      SELECT
        market_code,
        count
      FROM orders_no_preagg
      LIMIT 5
      """

      assert {:ok, results} = Connection.query(conn, query)
      materialized = Result.materialize(results)

      # Should have 2 columns
      assert length(materialized.data) == 2

      # Should have data
      first_column = hd(materialized.data)
      assert length(first_column.data) > 0
    end

    test "queries orders_no_preagg cube II", %{conn: conn} do
      query = """
      SELECT
      orders_no_preagg.market_code,
      MEASURE(orders_no_preagg.count)
      FROM
      orders_no_preagg
      GROUP BY
      1
      ORDER BY
      2 DESC
      LIMIT 249
      """

      assert {:ok, results} = Connection.query(conn, query)
      materialized = Result.materialize(results)
      IO.inspect(materialized)

      # Should have 2 columns
      assert length(materialized.data) == 2

      # Should have data
      first_column = hd(materialized.data)
      assert length(first_column.data) > 0
    end

    test "queries orders_no_preagg cube III", %{conn: conn} do
      query = """
      SELECT
      orders_no_preagg.market_code,
      MEASURE(orders_no_preagg.count)
      FROM
      orders_no_preagg
      WHERE (orders_no_preagg.market_code = 'VN')
      GROUP BY 1
      HAVING (MEASURE(orders_no_preagg.count) > '20000')
      ORDER BY 2 DESC
      """

      assert {:ok, results} = Connection.query(conn, query)
      materialized = Result.materialize(results)
      IO.inspect(materialized)

      # Should have 2 columns
      assert length(materialized.data) == 2

      # Should have data
      first_column = hd(materialized.data)
      assert length(first_column.data) > 0
    end
  end

  describe "error handling" do
    test "handles non-existent table error", %{conn: conn} do
      query = "SELECT * FROM nonexistent_table LIMIT 1"

      assert {:error, %Adbc.Error{} = error} = Connection.query(conn, query)
      # Verify detailed error message is passed through from CubeSQL
      assert error.message =~ "nonexistent_table"
      assert error.message =~ "not found"
    end

    test "handles invalid SQL syntax error", %{conn: conn} do
      query = "SELECT WHERE FROM"

      assert {:error, %Adbc.Error{} = error} = Connection.query(conn, query)
      # Verify detailed error message is passed through
      assert error.message =~ "parse" or error.message =~ "ParserError"
    end

    test "handles non-existent column error", %{conn: conn} do
      query = "SELECT nonexistent_column FROM orders_with_preagg LIMIT 1"

      assert {:error, %Adbc.Error{} = error} = Connection.query(conn, query)
      # Verify detailed error message is passed through
      assert error.message =~ "nonexistent_column" or error.message =~ "not found" or
               error.message =~ "Invalid identifier"
    end

    test "connection recovers after query errors", %{conn: conn} do
      # First, cause an error
      assert {:error, _} = Connection.query(conn, "SELECT * FROM nonexistent_table LIMIT 1")

      # Then verify connection still works with valid query
      assert {:ok, results} =
               Connection.query(conn, "SELECT market_code FROM orders_with_preagg LIMIT 1")

      materialized = Result.materialize(results)

      # Connection recovered - we got a result with the expected column
      assert %Result{
               data: [
                 %Column{
                   name: "market_code"
                 }
               ]
             } = materialized

      # Verify we got data back
      assert length(materialized.data) == 1
      assert hd(materialized.data).data != []
    end
  end
end
