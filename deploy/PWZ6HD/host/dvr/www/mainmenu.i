
<div style="padding-left: 80px;" ><p class="btset">
  <input name="btset" id="system" type="radio" /><label for="system">System</label>
  <input name="btset" id="camera" type="radio" /><label for="camera">Camera</label>
  <input name="btset" id="bodycam" type="radio" /><label for="bodycam">Bodycam</label>
  <input name="btset" id="sensor" type="radio" /><label for="sensor">Sensor</label>
  <input name="btset" id="network" type="radio" /><label for="network">Network</label>
  <input name="btset" id="status" type="radio" /><label for="status">Status</label>
  <input name="btset" id="tools" type="radio" /><label for="tools">Tools</label>
</p></div>

  <script>
  $( function() {
    $(".btset").buttonset();

    $("input[name='btset']").click(function(e){
        e.preventDefault()
        if( this.id == "system" ) on_system_click();
        else if( this.id == "camera" ) on_camera_click();
        else if( this.id == "bodycam" ) on_bodycam_click();
        else if( this.id == "sensor" ) on_sensor_click();
        else if( this.id == "network" ) on_network_click();
        else if( this.id == "status" ) on_status_click();
        else if( this.id == "tools" ) on_tools_click();
    });
  });
  </script>
