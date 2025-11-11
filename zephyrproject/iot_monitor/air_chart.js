// air_chart.js
// Handles fetching and chart updates for air quality and flame data

// =============== CONFIG ===============
const REFRESH_INTERVAL = 10000; // 10 seconds
// =====================================

// ✅ Generic data fetcher
async function fetchData(endpoint) {
  try {
    const res = await fetch('?fetch=' + endpoint);
    const rows = await res.json();
    return Array.isArray(rows) ? rows : [];
  } catch (err) {
    console.error('Error fetching ' + endpoint + ':', err);
    return [];
  }
}

// ✅ Chart builder (creates or refreshes a Chart.js line graph)
function buildChart(canvasId, label, color, labels, data) {
  const ctx = document.getElementById(canvasId)?.getContext('2d');
  if (!ctx) return; // Skip if canvas not found

  // Destroy old chart if exists
  if (window[canvasId]) window[canvasId].destroy();

  // Create new chart
  window[canvasId] = new Chart(ctx, {
    type: 'line',
    data: {
      labels: labels,
      datasets: [{
        label: label,
        data: data,
        borderColor: color,
        backgroundColor: color.replace('1)', '0.2)'),
        tension: 0.3,
        fill: true,
        pointRadius: 2
      }]
    },
    options: {
      responsive: true,
      scales: {
        x: {
          title: { display: true, text: 'Timestamp' },
          ticks: { maxRotation: 0, autoSkip: true, maxTicksLimit: 10 }
        },
        y: {
          beginAtZero: true,
          title: { display: true, text: label }
        }
      },
      plugins: {
        legend: { display: true, position: 'top' }
      }
    }
  });
}

// ✅ Update charts for air quality and flame data
async function updateCharts() {
  // --- Air Quality Chart ---
  const airData = await fetchData('air_recent');
  if (airData.length) {
    const airLabels = airData.map(r => r.id);
    const airValues = airData.map(r => Number(r.value));
    buildChart('aqChart', 'Air Quality Value', 'rgba(54, 162, 235, 1)', airLabels, airValues);
  }

  // --- Flame Chart (optional) ---
  const flameCanvas = document.getElementById('flameChart');
  if (flameCanvas) {
    const flameData = await fetchData('flame_recent');
    if (flameData.length) {
      const flameLabels = flameData.map(r => r.id);
      const flameValues = flameData.map(r => Number(r.status)); // 🔹 use 'status' not 'value'
      buildChart('flameChart', 'Flame Detection (1 = Fire)', 'rgba(255, 99, 132, 1)', flameLabels, flameValues);
    }
  }
}

// ✅ Run on load
document.addEventListener('DOMContentLoaded', () => {
  updateCharts();
  setInterval(updateCharts, REFRESH_INTERVAL);
});
