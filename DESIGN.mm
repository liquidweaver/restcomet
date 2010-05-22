<map version="0.9.0">
<!-- To view this file, download free mind mapping software FreeMind from http://freemind.sourceforge.net -->
<node CREATED="1274552239622" ID="ID_1511729653" MODIFIED="1274555230350" STYLE="bubble" TEXT="RESTful Comet">
<icon BUILTIN="idea"/>
<node CREATED="1274552504119" HGAP="-97" ID="ID_1198157643" MODIFIED="1274555341196" POSITION="right" STYLE="bubble" TEXT="client" VSHIFT="-143">
<node CREATED="1274554976658" ID="ID_1052457612" MODIFIED="1274554980974" TEXT="mechanics">
<node CREATED="1274552657463" ID="ID_761475943" MODIFIED="1274552685891" STYLE="bubble" TEXT="multi-event comet thread">
<node CREATED="1274552671989" ID="ID_794237133" MODIFIED="1274552685891" STYLE="bubble" TEXT="event dispatch"/>
</node>
</node>
<node CREATED="1274554317816" HGAP="27" ID="ID_84133354" MODIFIED="1274555344148" STYLE="bubble" TEXT="Behavior" VSHIFT="35">
<node CREATED="1274554748036" ID="ID_1089801039" MODIFIED="1274554949818" TEXT="If no sequence, client receives the next even that matchers filter set"/>
<node CREATED="1274554775012" ID="ID_491369432" MODIFIED="1274554949818" TEXT="If sequence given, all events happening after that sequence are returned that match given event filter set. If sequence not found, all events are returned"/>
<node CREATED="1274554811043" ID="ID_100014694" MODIFIED="1274554949822" TEXT="last known sequence number (not just of events matching set) is returned. Client stores this and provides it in subsequent requests"/>
</node>
</node>
<node CREATED="1274552510247" HGAP="-54" ID="ID_522501035" MODIFIED="1274556005612" POSITION="left" STYLE="bubble" TEXT="server" VSHIFT="57">
<node CREATED="1274552699893" HGAP="26" ID="ID_1128501065" MODIFIED="1274555112789" TEXT="mechanics" VSHIFT="-82">
<node CREATED="1274552567830" HGAP="27" ID="ID_768056352" MODIFIED="1274555321589" STYLE="bubble" TEXT="child connection handlers" VSHIFT="-46">
<node CREATED="1274553248465" ID="ID_828001133" MODIFIED="1274555203974" TEXT=" Are spawned from the socket dispatch thread"/>
<node CREATED="1274555167166" ID="ID_964144680" MODIFIED="1274555205246" TEXT="Use a wait condition for new event"/>
<node CREATED="1274555163275" ID="ID_691869525" MODIFIED="1274555206581" TEXT="On new event, reply back on the connection"/>
</node>
<node CREATED="1274552537958" ID="ID_1056505844" MODIFIED="1274555981104" STYLE="bubble" TEXT="event dispatch mechanism">
<node CREATED="1274555247760" ID="ID_1911503207" MODIFIED="1274555300475" TEXT="Inserts event into event map keyed by sequence number"/>
<node CREATED="1274555306191" ID="ID_1205142490" MODIFIED="1274555315563" TEXT="increments sequence number"/>
<node CREATED="1274555985514" ID="ID_1583282604" MODIFIED="1274555990757" TEXT="Called by a public method"/>
</node>
<node CREATED="1274552518695" HGAP="17" ID="ID_1910300269" MODIFIED="1274555319949" STYLE="bubble" TEXT="socket dispatch thread" VSHIFT="37">
<node CREATED="1274553454911" ID="ID_411659383" MODIFIED="1274553464298" TEXT="Creates child connection handlers"/>
<node CREATED="1274553469071" ID="ID_58495000" MODIFIED="1274553470666" TEXT="port 80"/>
</node>
</node>
<node CREATED="1274552720229" HGAP="56" ID="ID_1337499306" MODIFIED="1274556026853" TEXT="singleton class" VSHIFT="-65"/>
<node CREATED="1274556006617" ID="ID_584068221" MODIFIED="1274556372657" TEXT="data types">
<node COLOR="#cc6600" CREATED="1274556373175" ID="ID_377315217" MODIFIED="1274556411767">
<richcontent TYPE="NODE"><html>
  <head>
    
  </head>
  <body>
    <p>
      struct Event
    </p>
    <p>
      {
    </p>
    <p>
      &#160;&#160;&#160;string guid;&#160;
    </p>
    <p>
      &#160;&#160;&#160;time_t timestamp;
    </p>
    <p>
      &#160;&#160;&#160;&#160;string eventData; //JSON
    </p>
    <p>
      }
    </p>
  </body>
</html></richcontent>
</node>
<node COLOR="#cc6600" CREATED="1274556163944" ID="ID_356863904" MODIFIED="1274556237415">
<richcontent TYPE="NODE"><html>
  <head>
    
  </head>
  <body>
    <p>
      //uint is the sequence
    </p>
    <p>
      map&lt;uint, Event&gt; EventMap
    </p>
  </body>
</html></richcontent>
</node>
<node COLOR="#cc6600" CREATED="1274556435878" ID="ID_1136652556" MODIFIED="1274556449670" TEXT="uint eventSequence"/>
</node>
</node>
</node>
</map>
