/*
 * websocket_demo.js
 *
 * Copyright (c) 2019 Renesas Electronics Corp.
 * This software is released under the MIT License,
 * see https://opensource.org/licenses/MIT
 */

var socket = new WebSocket("ws://192.168.1.50:3000/", "graph-update");
var temp_ctx = document.getElementById("temp_canvas").getContext("2d");
var hum_ctx = document.getElementById("hum_canvas").getContext("2d");
var light_ctx = document.getElementById("light_canvas").getContext("2d");
var state = "auto";
var thresh = "";
var tempGradientFill = temp_ctx.createLinearGradient(0,0,0,450);
var humGradientFill = hum_ctx.createLinearGradient(0,0,0,450);
var lightGradientFill = light_ctx.createLinearGradient(0,0,0,450);

var temp_yaxes_max = document.getElementById("temp_yaxes_max");
var temp_yaxes_min = document.getElementById("temp_yaxes_min");

var light_yaxes_max = document.getElementById("light_yaxes_max");
var light_yaxes_min = document.getElementById("light_yaxes_min");

var led_icon = document.getElementById('led-icon');

var proximity_threshold = document.getElementById("proximity_threshold");
var proximity_threshold_value = proximity_threshold.value;
var proximity = 0;

tempGradientFill.addColorStop(0, 'rgba(255,255,255,1)');
tempGradientFill.addColorStop(1, 'rgba(255,255,255,0)');

humGradientFill.addColorStop(0, 'rgba(41,235,253,1)');
humGradientFill.addColorStop(1, 'rgba(41,235,253,0)');

lightGradientFill.addColorStop(0, 'rgba(253,192,4,1)');
lightGradientFill.addColorStop(1, 'rgba(253,192,4,0)');

temp_yaxes_max.addEventListener('keypress', update_temp_yaxes_max);
temp_yaxes_min.addEventListener('keypress', update_temp_yaxes_min);

light_yaxes_max.addEventListener('keypress', update_light_yaxes_max);
light_yaxes_min.addEventListener('keypress', update_light_yaxes_min);

proximity_threshold.addEventListener('keypress', update_proximity_threshold);

var tempChart = new Chart(temp_ctx, {
  type: "line",
  data: {
    labels: [],
    datasets: [
      {
        label: "Temperature [℃]",
        yAxisID: 'tempaxis',
        data: [],
        borderColor: 'rgb(255,255,255)',
        backgroundColor: tempGradientFill,
        pointBackgroundColor: '#006A92',
        borderWidth: 2
      }
    ],
  },
  options: {
    legend: {
      labels: {
        fontColor: '#FFF',
        fontSize: 15,
        boxWidth: 45
      },
    },
    scales: {
      yAxes: [{
        id: 'tempaxis',
        type: 'linear',
        position: 'left',
        scaleLabel: {
          display: true,
          labelString: 'Temperature [℃]',
          fontSize: 20,
          fontColor: '#FFF'
       },
        gridLines:{
            display: false,
       },
        ticks:{
            min: +temp_yaxes_min.value,
            max: +temp_yaxes_max.value,
            fontColor: '#FFF',
            fontSize: 15
       },
      }],
      xAxes:[{
        gridLines:{
          color: '#FFF',
          borderDash: [2, 2]
        },
        ticks:{
          fontColor: '#FFF',
          fontSize: 10
        },
    }]
  }
}
});

var humChart = new Chart(hum_ctx, {
    type: "line",
    data: {
        labels: [],
        datasets: [
            {
                label: "Humidity [%]",
                yAxisID: 'humaxis',
                data: [],
                borderColor: 'rgb(41,235,253)',
                backgroundColor: humGradientFill,
                pointBackgroundColor: '#FFF',
                borderWidth: 2
            }
        ],
    },
    options: {
        legend: {
            labels: {
                fontColor: '#FFF',
                fontSize: 15,
                boxWidth: 45
            },
        },
        scales: {
            yAxes: [{
                id: 'humaxis',
                type: 'linear',
                position: 'left',
                scaleLabel: {
                    display: true,
                    labelString: 'Humidity [%]',
                    fontSize: 20,
                    fontColor: '#FFF'
                },
                gridLines: {
                    display: false,
                },
                ticks: {
                    fontColor: '#FFF',
                    fontSize: 15,
                    precision: 0
                },
            }],
            xAxes: [{
                gridLines: {
                    color: '#FFF',
                    borderDash: [2, 2]
                },
                ticks: {
                    fontColor: '#FFF',
                    fontSize: 10
                },
            }]
        }
    }
});
var lightChart = new Chart(light_ctx, {
  type: "line",
  data: {
    labels: [],
    datasets: [
      {
        label: "Ambient Light [lx]",
        yAxisID: 'lightaxis',
        data: [],
        borderColor: 'rgb(253,192,4)',
        backgroundColor: lightGradientFill,
        pointBackgroundColor: '#FFF',
        borderWidth: 2
      }
    ],
  },
  options: {
    legend: {
      labels: {
        fontColor: '#FFF',
        fontSize: 15,
        boxWidth: 45
      },
    },
    scales: {
      yAxes: [{
        id: 'lightaxis',
        type: 'linear',
        position: 'left',
        scaleLabel: {
          display: true,
          labelString: 'Ambient Light [lx]',
          fontSize: 20,
          fontColor: '#FFF'
       },
        gridLines:{
          display: false,
       },
        ticks:{
          min: +light_yaxes_min.value,
          max: +light_yaxes_max.value,
          fontColor: '#FFF',
          fontSize: 15
       },
      }],
      xAxes:[{
        gridLines:{
          color: '#FFF',
          borderDash: [2, 2]
        },
        ticks:{
          fontColor: '#FFF',
          fontSize: 10
        },
    }]
  }
}
});

$(() => {
  socket.onmessage = function(event) {
    var datas = JSON.parse(event.data);
    var time = moment();
    var outputTime = time.format("HH : mm : ss");

    if(datas.temp.isActive == true){
      tempChart.data.labels.push(outputTime);
      tempChart.data.datasets[0].data.push(datas.temp.value);

      if (tempChart.data.labels.length > 10){
        tempChart.data.labels.shift();
        tempChart.data.datasets.forEach((dataset) => {
            dataset.data.shift();
        });
      }

      tempChart.update();
      $("#tempcell").text(datas.temp.value+" ℃");
    }

    if(datas.humm.isActive == true){
      humChart.data.labels.push(outputTime);
      humChart.data.datasets[0].data.push(datas.humm.value);

      if (humChart.data.labels.length > 10){
        humChart.data.labels.shift();
        humChart.data.datasets.forEach((dataset) => {
            dataset.data.shift();
        });
      }

      humChart.update();
      $("#humcell").text(datas.humm.value+" %");
    }

    if(datas.light.isActive == true){
      lightChart.data.labels.push(outputTime);
      lightChart.data.datasets[0].data.push(datas.light.value);

      if (lightChart.data.labels.length > 10){
        lightChart.data.labels.shift();
        lightChart.data.datasets.forEach((dataset) => {
            dataset.data.shift();
        });
      }

      lightChart.update();
      $("#lightcell").text(datas.light.value+" lx");
    }

    if(datas.proximity.isActive == true){
      proximity = datas.proximity.value;
      $("#proximitycell").text(proximity);

      if(proximity >= proximity_threshold_value) {
        socket.send(JSON.stringify({
          led: "on"
        }));
        led_icon.src = "img/icon_led-on.png";
      } else {
        socket.send(JSON.stringify({
          led: "off"
        }));
        led_icon.src = "img/icon_led-off.png";
      }
    }
  };
});

function update_temp_yaxes_max(e) {
  if(e.keyCode === 13) { // input enter key
    tempChart.options.scales.yAxes[0].ticks.max = +temp_yaxes_max.value;
    tempChart.update();
  }
}

function update_temp_yaxes_min(e) {
  if(e.keyCode === 13) {  // input enter key
    tempChart.options.scales.yAxes[0].ticks.min = +temp_yaxes_min.value;
    tempChart.update();
  }
}

function update_light_yaxes_max(e) {
  if(e.keyCode === 13) {  // input enter key
    lightChart.options.scales.yAxes[0].ticks.max = +light_yaxes_max.value;
    lightChart.update();
  }
}

function update_light_yaxes_min(e) {
  if(e.keyCode === 13) {  // input enter key
    lightChart.options.scales.yAxes[0].ticks.min = +light_yaxes_min.value;
    lightChart.update();
  }
}

function update_proximity_threshold(e) {
  if(e.keyCode === 13) {  // input enter key
    proximity_threshold_value = +proximity_threshold.value;
  }
}

document.addEventListener('DOMContentLoaded', function(){

  const settingIcon = document.getElementById('settings-icon');
  const settingMenu = document.getElementById('settings');

  function iconToggle() {
    settingIcon.classList.toggle('icon-change');
    if(settingIcon.className == 'icon-change'){
      settingIcon.src = "img/settings_close.png";
    }else{
      settingIcon.src = "img/settings_open.png";
    }
    settingMenu.classList.toggle('settings-open');
  }

  const settingsEvent = document.getElementsByClassName('settings-event');
  for(let i = 0; i < settingsEvent.length; i++) {
    settingsEvent[i].addEventListener('click', iconToggle, false);
  }
}, false);
