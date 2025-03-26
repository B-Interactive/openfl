package openfl.media._internal;
import cpp.Pointer;
import cpp.RawPointer;
import cpp.UInt8;
import haxe.io.Bytes;
import haxe.io.BytesData;
/**
 * ...
 * @author Christopher Speciale
 */
@:include('./NativeVideoBackend.cpp')
extern class NativeVideoBackend 
{
	@:native('video_init') private static function __videoInit():Bool;	
	@:native('video_software_load') private static function __videoSoftwareLoad(path:String, buffer:Pointer<UInt8>, length:Int):Bool;	
	@:native('video_gl_load') private static function __videoGLLoad(path:String):Bool;	
	@:native('video_gl_update_frame') private static function __videoGLUpdateFrame():Bool;	
	@:native('video_software_update_frame') private static function __videoSoftwareUpdateFrame():Bool;	
	@:native('video_get_frame_pixels') private static function __videoGetFramePixels(width:Pointer<Int>, height:Pointer<Int>):RawPointer<UInt8>;	
	@:native('video_shutdown') private static function __videoShutdown():Void;
	@:native('video_gl_get_texture_id_y') private static function __getTextureIDY():Int;
	@:native('video_gl_get_texture_id_uv') private static function __getTextureIDUV():Int;
	@:native('video_get_width') private static function __videoGetWidth(path:String):Int;
	@:native('video_get_height') private static function __videoGetHeight(path:String):Int;
	
}