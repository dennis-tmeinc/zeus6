$( document ).on( "pagecreate", function() {

    $( "#btn_showkey").on( "click", function( event ) {
      event.preventDefault();
      var key=$( "input#wifi_key").val();
      var type=$( "input#wifi_key").attr("type");
      if( type=="password") {
        $( "input#wifi_key").attr("type","text");
        $( "#btn_showkey").html("Hide Key");
      }
      else {
        $( "input#wifi_key").attr("type","password");
        $( "#btn_showkey").html("Show Key");
      }
    });

    function setfieldvalue( fieldid, fieldvalue )
    {
        try {
            var elems = document.getElementsByName( fieldid ) ;
            if( elems != null )
            for( var i=0; i<elems.length; i++) {
                if( elems[i].type == "checkbox" ) {
                  elems[i].checked = ( fieldvalue == elems[i].value ) || ( fieldvalue=="on" ) || (fieldvalue==1) ;
                }
                else if( elems[i].type == "radio" ) {
                  elems[i].checked = ( fieldvalue == elems[i].value ) ;
                }
                else {
                  elems[i].value = fieldvalue ;
                }
            }
        }
        catch( err )
        {
            alert( "JavaScript Error: setfieldvalue() "+err.description+"\n\nClick OK to continue.\n\n");
        }
    }

    // load form form data
    var pagename = $("input[name='page']").val() ;
    if( pagename == "ss" ) {
      $.mobile.navigate("system1.html");
    }
    else if( pagename == "network") {
      $.getJSON( "/network_value", function( data ) {
        if( data ) {
            for( f in data ) {
                setfieldvalue( f, data[f] );
            }
        }
      });
    }

    $(document).off( "pagebeforehide" );
    $(document).on( "pagebeforehide", function( event ) {
      var page = $("input[name='page']").val() ;
      var formdata = 0;

    });

});
