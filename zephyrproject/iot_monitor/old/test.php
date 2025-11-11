<?php
// index.php
require_once 'db.php';

header('Content-Type: text/html; charset=utf-8');

// ONLY process JSON endpoints if specifically requested
if (isset($_GET['fetch'])) {
    if ($_GET['fetch'] === 'air_recent') {
        // return last 200 air readings
        $limit = 200;
        $stmt = $pdo->prepare("SELECT id, value, recorded_at FROM air_quality ORDER BY recorded_at DESC LIMIT :l");
        $stmt->bindValue(':l', (int)$limit, PDO::PARAM_INT);
        $stmt->execute();
        $rows = $stmt->fetchAll();
        // return in chronological order
        echo json_encode(array_reverse($rows));
        exit;
    }
    if ($_GET['fetch'] === 'flame_recent') {
        $limit = 200;
        $stmt = $pdo->prepare("SELECT id, status, recorded_at FROM flame_events ORDER BY recorded_at DESC LIMIT :l");
        $stmt->bindValue(':l', (int)$limit, PDO::PARAM_INT);
        $stmt->execute();
        $rows = $stmt->fetchAll();
        echo json_encode($rows);
        exit;
    }
}

// If we get here, it means it's NOT a JSON request, so render HTML
$page = $_GET['page'] ?? 'air';

// helper to get latest air value for display
function get_latest_air($pdo)
{
    $stmt = $pdo->query("SELECT value, recorded_at FROM air_quality ORDER BY recorded_at DESC LIMIT 1");
    return $stmt->fetch();
}

// helper to check if any flame detected in last N minutes
function flame_in_last_minutes($pdo, $minutes = 10)
{
    $stmt = $pdo->prepare("SELECT COUNT(*) as c FROM flame_events WHERE status = 1 AND recorded_at >= (NOW() - INTERVAL :m MINUTE)");
    $stmt->bindValue(':m', (int)$minutes, PDO::PARAM_INT);
    $stmt->execute();
    $row = $stmt->fetch();
    return (int)$row['c'] > 0;
}

$latest = get_latest_air($pdo);
$recentFlame = flame_in_last_minutes($pdo, 30);

?>
<!doctype html>
<html lang="en">

<head>
	<meta charset="utf-8">
	<meta name="viewport" content="width=device-width, initial-scale=1">
	<title>IoT Monitor - Air & Flame</title>
	<link href="https://cdn.jsdelivr.net/npm/bootstrap@5.3.2/dist/css/bootstrap.min.css" rel="stylesheet">
	<script src="https://cdn.jsdelivr.net/npm/chart.js@4.4.0/dist/chart.umd.min.js"></script>
	<style>
		body {
			padding-top: 70px;
		}

		.value-card {
			font-size: 2.25rem;
			font-weight: 700;
		}
	</style>
</head>

<body>
	<nav class="navbar navbar-expand-lg navbar-dark bg-dark fixed-top">
		<div class="container-fluid">
			<a class="navbar-brand" href="./">IoT Monitor</a>
			<button class="navbar-toggler" type="button" data-bs-toggle="collapse" data-bs-target="#navMenu">
				<span class="navbar-toggler-icon"></span>
			</button>
			<div class="collapse navbar-collapse" id="navMenu">
				<ul class="navbar-nav me-auto">
					<li class="nav-item"><a
							class="nav-link <?= $page === 'air' ? 'active' : '' ?>"
							href="?page=air">Air Quality</a></li>
					<li class="nav-item"><a
							class="nav-link <?= $page === 'flame' ? 'active' : '' ?>"
							href="?page=flame">Flame Events</a></li>
					<li class="nav-item"><a class="nav-link" href="README.txt" target="_blank">Readme</a></li>
				</ul>
				<span class="navbar-text text-muted">Local XAMPP / MySQL</span>
			</div>
		</div>
	</nav>

	<div class="container">
		<?php if ($recentFlame): ?>
		<div class="alert alert-danger d-flex align-items-center" role="alert">
			<svg class="bi flex-shrink-0 me-2" width="24" height="24" role="img" aria-label="Danger:">
				<use xlink:href="#exclamation-triangle-fill" />
			</svg>
			<div>
				Flame detected in the last 30 minutes — check flame events page for details.
			</div>
		</div>
		<?php endif; ?>

		<?php if ($page === 'air'): ?>
		<div class="row mb-3">
			<div class="col-md-8">
				<div class="card">
					<div class="card-header">Air Quality (recent)</div>
					<div class="card-body">
						<canvas id="aqChart" height="120"></canvas>
					</div>
				</div>
			</div>
			<div class="col-md-4">
				<div class="card mb-3">
					<div class="card-body">
						<h6 class="card-title">Latest Air Value</h6>
						<div class="value-card text-center">
							<?php if ($latest): ?>
							<div id="latestVal">
								<?= htmlspecialchars($latest['value']) ?>
							</div>
							<div class="small text-muted">at
								<?= htmlspecialchars($latest['recorded_at']) ?>
							</div>
							<?php else: ?>
							<div class="text-muted">No data yet</div>
							<?php endif; ?>
						</div>
					</div>
				</div>

				<div class="card">
					<div class="card-header">Complaint / Status</div>
					<div class="card-body" id="aqComplaint">
						<?php
              // simple rule: value > 300 => complaint (adjust for your sensor)
              $complaint = '';
if ($latest) {
    $val = (float)$latest['value'];
    if ($val >= 300) {
        $complaint = "<div class='alert alert-warning'>High pollution detected (value = $val). Raise alarm / ventilation.</div>";
    } elseif ($val >= 150) {
        $complaint = "<div class='alert alert-info'>Moderate pollution (value = $val). Monitor closely.</div>";
    } else {
        $complaint = "<div class='text-success'>Air quality normal (value = $val).</div>";
    }
} else {
    $complaint = "<div class='text-muted'>No data yet.</div>";
}
echo $complaint;
?>
					</div>
				</div>

			</div>
		</div>

		<script>
			// Fetch data and plot
			async function fetchAirData() {
				const res = await fetch('?fetch=air_recent');
				const rows = await res.json();
				return rows;
			}

			function buildChart(labels, data) {
				const ctx = document.getElementById('aqChart').getContext('2d');
				if (window.aqChart) window.aqChart.destroy();
				window.aqChart = new Chart(ctx, {
					type: 'line',
					data: {
						labels: labels,
						datasets: [{
							label: 'Air Value',
							data: data,
							tension: 0.2,
							fill: true,
							pointRadius: 1
						}]
					},
					options: {
						scales: {
							x: {
								display: true,
								ticks: {
									maxRotation: 0,
									autoSkip: true,
									maxTicksLimit: 12
								}
							},
							y: {
								beginAtZero: true
							}
						},
						plugins: {
							legend: {
								display: false
							}
						}
					}
				});
			}

			(async () => {
				const rows = await fetchAirData();
				const labels = rows.map(r => r.recorded_at);
				const values = rows.map(r => Number(r.value));
				buildChart(labels, values);
				// update latest displayed value every 10s
				setInterval(async () => {
					const r = await fetchAirData();
					if (r.length) {
						document.getElementById('latestVal').innerText = r[r.length - 1].value;
					}
				}, 10000);
			})();
		</script>

		<?php elseif ($page === 'flame'): ?>
		<div class="row">
			<div class="col-md-8">
				<div class="card mb-3">
					<div class="card-header">Recent Flame Events</div>
					<div class="card-body">
						<table class="table table-sm" id="flameTable">
							<thead>
								<tr>
									<th>#</th>
									<th>Status</th>
									<th>Time</th>
								</tr>
							</thead>
							<tbody></tbody>
						</table>
						<div id="noFlameMsg" class="text-muted">Loading...</div>
					</div>
				</div>
			</div>

			<div class="col-md-4">
				<div class="card">
					<div class="card-header">Quick Actions</div>
					<div class="card-body">
						<p class="small">API endpoints (GET):</p>
						<pre class="small">/api/air.php?api_key=YOUR_KEY&value=123.4
/api/flame.php?api_key=YOUR_KEY&status=1</pre>
						<p class="small text-muted">Use your device / ESP AT commands to call the above URLs.</p>
					</div>
				</div>
			</div>
		</div>

		<script>
			async function fetchFlame() {
				const res = await fetch('?fetch=flame_recent');
				return await res.json();
			}

			(async () => {
				const rows = await fetchFlame();
				const tbody = document.querySelector('#flameTable tbody');
				tbody.innerHTML = '';
				if (!rows.length) {
					document.getElementById('noFlameMsg').innerText = 'No flame events yet.';
					return;
				}
				document.getElementById('noFlameMsg').innerText = '';
				rows.forEach((r, idx) => {
					const tr = document.createElement('tr');
					const statusText = r.status == 1 ? '<span class="badge bg-danger">FLAME</span>' :
						'<span class="badge bg-secondary">OK</span>';
					tr.innerHTML = `<td>${r.id}</td><td>${statusText}</td><td>${r.recorded_at}</td>`;
					tbody.appendChild(tr);
				});
			})();
		</script>

		<?php endif; ?>
	</div>

	<!-- bootstrap js bundle -->
	<script src="https://cdn.jsdelivr.net/npm/bootstrap@5.3.2/dist/js/bootstrap.bundle.min.js"></script>
</body>

</html>