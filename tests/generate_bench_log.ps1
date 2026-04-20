param(
    [string]$OutputPath = "",
    [int]$Repeat = 1800
)

if ([string]::IsNullOrWhiteSpace($OutputPath)) {
    $OutputPath = Join-Path $PSScriptRoot "data\\bench_mixed_5mb.log"
}

$templates = @(
    "2026-04-20 10:00:00.101 INFO  [gateway] [reqId={0}] Incoming request method=POST path=/api/v1/orders from=10.10.2.21",
    "2026-04-20 10:00:00.128 INFO  [auth] [reqId={0}] Token validated userId=10086",
    "2026-04-20 10:00:00.205 WARN  [inventory] [reqId={0}] Redis latency high costMs=248 key=inv:SPX-13",
    "2026-04-20 10:00:00.366 ERROR [inventory] [reqId={0}] Upstream timeout service=inventory-core timeoutMs=500",
    "2026-04-20 10:00:00.371 ERROR [gateway] [reqId={0}] Request failed status=500 costMs=703 errCode=ORDER_STOCK_RESERVE_FAIL",
    "2026-04-20 10:00:01.014 INFO  [gateway] [reqId={1}] Incoming request method=GET path=/api/v1/orders/98321 from=10.10.2.22",
    "2026-04-20 10:00:01.042 WARN  [db] [reqId={1}] Slow query costMs=912 sql=select * from orders where id=?",
    "2026-04-20 10:00:01.489 ERROR [db] [reqId={1}] transaction rollback because deadlock victim tx=order-detail-read",
    "2026-04-20 10:00:01.491 ERROR [gateway] [reqId={1}] Request failed status=500 costMs=949 errCode=ORDER_QUERY_DB_DEADLOCK",
    "2026-04-20 10:00:02.015 INFO  [gateway] Incoming request method=POST path=/api/v1/payment/callback from=10.10.8.18",
    "2026-04-20 10:00:02.121 WARN  [payment] Idempotent check delayed costMs=214 orderId=98321",
    "2026-04-20 10:00:02.662 ERROR [payment] Partner timeout upstream=partner-confirm timeoutMs=800 errCode=PAYMENT_CALLBACK_CONFIRM_TIMEOUT",
    "2026-04-20 10:00:03.014 INFO  [gateway] [reqId={2}] Request completed status=200 costMs=126 path=/api/v1/products/42",
    "2026-04-20 10:00:03.118 DEBUG [infra] pool=order-worker active=12 queue=1",
    "2026-04-20 10:00:03.241 WARN  [sms] send coupon message slow costMs=401 template=pay_success"
)

$dir = Split-Path -Parent $OutputPath
if (-not (Test-Path -LiteralPath $dir)) {
    New-Item -ItemType Directory -Path $dir | Out-Null
}

$writer = [System.IO.StreamWriter]::new($OutputPath, $false, [System.Text.Encoding]::UTF8)
try {
    for ($i = 0; $i -lt $Repeat; $i++) {
        $reqA = ("ord{0:D5}" -f $i)
        $reqB = ("qry{0:D5}" -f $i)
        $reqC = ("ok{0:D5}" -f $i)
        foreach ($tpl in $templates) {
            $writer.WriteLine([string]::Format($tpl, $reqA, $reqB, $reqC))
        }
    }
}
finally {
    $writer.Dispose()
}

Get-Item -LiteralPath $OutputPath | Select-Object FullName, Length
