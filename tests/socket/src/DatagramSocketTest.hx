package;

import openfl.events.DatagramSocketDataEvent;
import openfl.net.DatagramSocket;
#if (sys || air)
	import openfl.utils.ByteArray;
	import openfl.errors.ArgumentError;
	import openfl.errors.IllegalOperationError;
#end
import utest.Assert;
import utest.Async;
import utest.Test;

/**
 *  UDP unit-tests for openfl.net.DatagramSocket
 *  Requires a desktop/AIR target (`hl`, `neko`, `cpp`, `air`).
 */
class DatagramSocketTest extends Test
{
	#if (sys || air)
	var sockA:DatagramSocket; // “server” conceptually
	var sockB:DatagramSocket; // “client”

	/* ---------- helpers ---------- */

	inline function makeSocket():DatagramSocket
	{
		var s = new DatagramSocket();
		#if air
		s.bind(0, "127.0.0.1");
		#else
		s.bind();                    // auto address/port
		#end
		s.receive();                 // start listening immediately
		return s;
	}

	inline function close(s:DatagramSocket):Void
	{
		if (s != null && s.bound) s.close();
	}

	/* ---------- teardown ---------- */

	public function teardown()
	{
		close(sockA);
		close(sockB);
		sockA = sockB = null;
	}

	/* ---------- tests ---------- */

	public function test_bind()
	{
		sockA = makeSocket();
		Assert.isTrue(sockA.bound);
		Assert.notNull(sockA.localAddress);
		Assert.isTrue(sockA.localPort > 0);
		close(sockA);
		Assert.isFalse(sockA.bound);
	}

	@:timeout(2000)
	public function test_sendReceive(async:Async)
	{
		sockA = makeSocket();
		sockB = makeSocket();

		var payload = "PING";
		var bytes   = new ByteArray();
		bytes.writeUTFBytes(payload);

		// B receives
		sockB.addEventListener(DatagramSocketDataEvent.DATA, function(e)
		{
			if (async.timedOut) return;

			Assert.equals(payload, e.data.toString());
			async.done();
		});

		sockA.send(bytes, 0, 0, sockB.localAddress, sockB.localPort);
	}

	@:timeout(2000)
	public function test_bidirectionalEcho(async:Async)
	{
		sockA = makeSocket();
		sockB = makeSocket();

		var aMsg = "HELLO";
		var bMsg = "WORLD";

		var aBytes = new ByteArray(); aBytes.writeUTFBytes(aMsg);
		var bBytes = new ByteArray(); bBytes.writeUTFBytes(bMsg);

		var gotA = false;
		var gotB = false;

		function check() if (gotA && gotB && !async.timedOut) async.done();

		sockA.addEventListener(DatagramSocketDataEvent.DATA, function(e)
		{
			Assert.equals(bMsg, e.data.toString());
			gotA = true; check();
		});

		sockB.addEventListener(DatagramSocketDataEvent.DATA, function(e)
		{
			Assert.equals(aMsg, e.data.toString());
			gotB = true; check();
		});

		sockA.send(aBytes, 0, 0, sockB.localAddress, sockB.localPort);
		sockB.send(bBytes, 0, 0, sockA.localAddress, sockA.localPort);
	}

	public function test_enableBroadcast_defaultValue()
	{
		sockA = makeSocket();
		Assert.isFalse(sockA.enableBroadcast);
	}

	#if (cpp || neko)
	public function test_enableBroadcast_setter()
	{
		sockA = makeSocket();
		
		// Should not throw on supported platforms
		sockA.enableBroadcast = true;
		Assert.isTrue(sockA.enableBroadcast);
		
		sockA.enableBroadcast = false;
		Assert.isFalse(sockA.enableBroadcast);
	}

	public function test_broadcastSend_requiresEnableBroadcast()
	{
		sockA = makeSocket();
		var bytes = new ByteArray();
		bytes.writeUTFBytes("BROADCAST");
		
		// Should throw error when trying to send to broadcast without enabling it
		var caught = false;
		try
		{
			sockA.send(bytes, 0, 0, "255.255.255.255", 9999);
		}
		catch (e:ArgumentError)
		{
			caught = true;
			Assert.isTrue(e.message.indexOf("broadcast") > -1);
		}
		Assert.isTrue(caught);
	}

	public function test_broadcastSend_withEnableBroadcast()
	{
		sockA = makeSocket();
		sockA.enableBroadcast = true;
		
		var bytes = new ByteArray();
		bytes.writeUTFBytes("BROADCAST");
		
		// Should not throw when broadcast is enabled (though packet may not be delivered)
		// We can't easily test actual broadcast reception in unit tests
		try
		{
			sockA.send(bytes, 0, 0, "255.255.255.255", 9999);
			Assert.pass();
		}
		catch (e:Dynamic)
		{
			// Some systems may not allow broadcast even when enabled
			// This is acceptable as long as we don't get our validation error
			if (Std.isOfType(e, ArgumentError) && e.message.indexOf("broadcast") > -1)
			{
				Assert.fail("Should not get broadcast validation error when broadcast is enabled");
			}
		}
	}

	public function test_broadcastAddressDetection()
	{
		sockA = makeSocket();
		var bytes = new ByteArray();
		bytes.writeUTFBytes("TEST");
		
		// Test various broadcast addresses
		var broadcastAddresses = ["255.255.255.255", "192.168.1.255", "10.0.0.255"];
		
		for (addr in broadcastAddresses)
		{
			var caught = false;
			try
			{
				sockA.send(bytes, 0, 0, addr, 9999);
			}
			catch (e:ArgumentError)
			{
				caught = true;
				Assert.isTrue(e.message.indexOf("broadcast") > -1);
			}
			Assert.isTrue(caught, 'Should detect $addr as broadcast address');
		}
	}

	public function test_regularSend_stillWorks()
	{
		sockA = makeSocket();
		sockB = makeSocket();
		
		// Enable broadcast on A but send to regular address
		sockA.enableBroadcast = true;
		
		var bytes = new ByteArray();
		bytes.writeUTFBytes("REGULAR");
		
		// Should work normally
		try
		{
			sockA.send(bytes, 0, 0, sockB.localAddress, sockB.localPort);
			Assert.pass();
		}
		catch (e:Dynamic)
		{
			Assert.fail("Regular send should work when broadcast is enabled");
		}
	}
	#else
	public function test_enableBroadcast_unsupportedPlatform()
	{
		sockA = makeSocket();
		
		// Should throw error on unsupported platforms
		var caught = false;
		try
		{
			sockA.enableBroadcast = true;
		}
		catch (e:IllegalOperationError)
		{
			caught = true;
			Assert.isTrue(e.message.indexOf("not supported") > -1);
		}
		Assert.isTrue(caught);
	}

	public function test_broadcastSend_unsupportedPlatform()
	{
		sockA = makeSocket();
		var bytes = new ByteArray();
		bytes.writeUTFBytes("BROADCAST");
		
		// Should throw error when trying to send to broadcast on unsupported platform
		var caught = false;
		try
		{
			sockA.send(bytes, 0, 0, "255.255.255.255", 9999);
		}
		catch (e:IllegalOperationError)
		{
			caught = true;
			Assert.isTrue(e.message.indexOf("not supported") > -1);
		}
		Assert.isTrue(caught);
	}
	#end

	#else
	/*  Non-sys targets (html5, mobile) – DatagramSocket unavailable. */
	public function test_placeholder()
	{
		Assert.pass();
	}
	#end
}