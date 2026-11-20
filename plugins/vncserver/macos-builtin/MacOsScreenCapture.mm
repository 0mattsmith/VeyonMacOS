/*
 * MacOsScreenCapture.mm - captures the main display of the current session with ScreenCaptureKit
 *
 * Copyright (c) 2026 Veyon Community
 *
 * This file is part of Veyon - https://veyon.io
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public
 * License along with this program (see COPYING); if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 *
 */

// ScreenCaptureKit is an Objective-C API built around blocks and protocols, so unlike the other macOS code of
// Veyon (which uses the Objective-C runtime from plain C++) this file is Objective-C++ - it is compiled with ARC
// and deliberately does not use Qt

#import <CoreGraphics/CoreGraphics.h>
#import <CoreMedia/CoreMedia.h>
#import <CoreVideo/CoreVideo.h>
#import <Foundation/Foundation.h>
#import <ScreenCaptureKit/ScreenCaptureKit.h>

#include <algorithm>
#include <atomic>
#include <cmath>
#include <cstring>
#include <mutex>

#include "MacOsScreenCapture.h"


namespace {

constexpr int64_t StartTimeoutNanoseconds = 10 * NSEC_PER_SEC;
constexpr int64_t StopTimeoutNanoseconds = 5 * NSEC_PER_SEC;
// longer side of the captured picture in pixels at most
constexpr double MaximumCaptureSize = 1920;
// changes are detected in tiles of this size
constexpr int TileSize = 32;
// beyond this many dirty rectangles it is cheaper to update the whole frame
constexpr size_t MaximumDirtyRects = 512;

}


struct MacOsScreenCapture::Private
{
	std::mutex mutex;
	std::vector<uint8_t> pixels;
	int width{0};
	int height{0};
	std::vector<Rect> dirtyRects;
	bool hasNewFrame{false};
	std::string lastError;

	std::atomic<bool> running{false};

	SCStream* stream{nil};
	id output{nil};
	dispatch_queue_t queue{nullptr};

	void setError( NSError* error, const char* context )
	{
		std::lock_guard<std::mutex> lock( mutex );
		lastError = std::string( context ) + ": " +
					( error ? std::string( error.localizedDescription.UTF8String ) : std::string( "unknown error" ) );
	}

	void addDirtyRect( const Rect& rect )
	{
		if( dirtyRects.size() >= MaximumDirtyRects )
		{
			dirtyRects.assign( 1, Rect{ 0, 0, width, height } );
			return;
		}
		dirtyRects.push_back( rect );
	}

	void copyRect( const uint8_t* source, size_t sourceBytesPerRow, const Rect& rect )
	{
		const auto targetBytesPerRow = size_t(width) * 4;
		for( int y = rect.y; y < rect.y + rect.height; ++y )
		{
			std::memcpy( pixels.data() + size_t(y) * targetBytesPerRow + size_t(rect.x) * 4,
						 source + size_t(y) * sourceBytesPerRow + size_t(rect.x) * 4,
						 size_t(rect.width) * 4 );
		}
	}

	// ScreenCaptureKit reports whole window surfaces as dirty (e.g. an entire browser window for a blinking cursor),
	// so compare the reported area tile by tile with the previous frame and only report tiles which really changed -
	// otherwise every update would be close to a full screen, which e.g. exceeds the bandwidth limit of demos
	void copyChangedTiles( const uint8_t* source, size_t sourceBytesPerRow, const Rect& rect )
	{
		const auto targetBytesPerRow = size_t(width) * 4;
		const int right = rect.x + rect.width;
		const int bottom = rect.y + rect.height;

		for( int tileY = rect.y / TileSize * TileSize; tileY < bottom; tileY += TileSize )
		{
			const int tileHeight = std::min( TileSize, height - tileY );
			int runStart = -1;
			int runEnd = -1;

			for( int tileX = rect.x / TileSize * TileSize; tileX < right; tileX += TileSize )
			{
				const int tileWidth = std::min( TileSize, width - tileX );
				const auto rowBytes = size_t(tileWidth) * 4;

				bool changed = false;
				for( int y = tileY; y < tileY + tileHeight; ++y )
				{
					auto target = pixels.data() + size_t(y) * targetBytesPerRow + size_t(tileX) * 4;
					const auto sourceRow = source + size_t(y) * sourceBytesPerRow + size_t(tileX) * 4;
					if( changed || std::memcmp( target, sourceRow, rowBytes ) != 0 )
					{
						changed = true;
						std::memcpy( target, sourceRow, rowBytes );
					}
				}

				// merge horizontally adjacent changed tiles into one rectangle
				if( changed )
				{
					if( runStart < 0 )
					{
						runStart = tileX;
					}
					runEnd = tileX + tileWidth;
				}
				else if( runStart >= 0 )
				{
					addDirtyRect( Rect{ runStart, tileY, runEnd - runStart, tileHeight } );
					runStart = -1;
				}
			}

			if( runStart >= 0 )
			{
				addDirtyRect( Rect{ runStart, tileY, runEnd - runStart, tileHeight } );
			}
		}
	}

	void storeFrame( CVPixelBufferRef pixelBuffer, NSArray* changedRects )
	{
		if( CVPixelBufferLockBaseAddress( pixelBuffer, kCVPixelBufferLock_ReadOnly ) != kCVReturnSuccess )
		{
			return;
		}

		const auto frameWidth = int( CVPixelBufferGetWidth( pixelBuffer ) );
		const auto frameHeight = int( CVPixelBufferGetHeight( pixelBuffer ) );
		const auto sourceBytesPerRow = CVPixelBufferGetBytesPerRow( pixelBuffer );
		const auto source = static_cast<const uint8_t*>( CVPixelBufferGetBaseAddress( pixelBuffer ) );

		if( source && frameWidth > 0 && frameHeight > 0 )
		{
			std::lock_guard<std::mutex> lock( mutex );

			std::vector<Rect> rects;
			const bool sizeChanged = frameWidth != width || frameHeight != height;
			if( sizeChanged || changedRects == nil )
			{
				rects.push_back( Rect{ 0, 0, frameWidth, frameHeight } );
			}
			else
			{
				// ScreenCaptureKit delivers the dirty rectangles as CGRect dictionary representations
				for( id value in changedRects )
				{
					CGRect cgRect = CGRectZero;
					if( [value isKindOfClass:[NSDictionary class]] )
					{
						CGRectMakeWithDictionaryRepresentation( (__bridge CFDictionaryRef)value, &cgRect );
					}
					else if( [value isKindOfClass:[NSValue class]] )
					{
						cgRect = NSRectToCGRect( [(NSValue*)value rectValue] );
					}
					cgRect = CGRectIntegral( CGRectIntersection( cgRect, CGRectMake( 0, 0, frameWidth, frameHeight ) ) );
					if( CGRectIsEmpty( cgRect ) == false )
					{
						rects.push_back( Rect{ int(cgRect.origin.x), int(cgRect.origin.y),
											   int(cgRect.size.width), int(cgRect.size.height) } );
					}
				}
			}

			if( sizeChanged )
			{
				width = frameWidth;
				height = frameHeight;
				pixels.assign( size_t(width) * size_t(height) * 4, 0 );
				dirtyRects.clear();
			}

			const auto dirtyRectsBefore = dirtyRects.size();

			if( sizeChanged )
			{
				copyRect( source, sourceBytesPerRow, rects.front() );
				addDirtyRect( rects.front() );
			}
			else
			{
				for( const auto& rect : rects )
				{
					copyChangedTiles( source, sourceBytesPerRow, rect );
				}
			}

			hasNewFrame = hasNewFrame || dirtyRects.size() != dirtyRectsBefore;
		}

		CVPixelBufferUnlockBaseAddress( pixelBuffer, kCVPixelBufferLock_ReadOnly );
	}
};



@interface VeyonScreenCaptureOutput : NSObject <SCStreamOutput, SCStreamDelegate>
{
	MacOsScreenCapture::Private* m_capture;
}
- (instancetype)initWithCapture:(MacOsScreenCapture::Private*)capture;
@end

@implementation VeyonScreenCaptureOutput

- (instancetype)initWithCapture:(MacOsScreenCapture::Private*)capture
{
	self = [super init];
	if( self )
	{
		m_capture = capture;
	}
	return self;
}

- (void)stream:(SCStream*)stream didOutputSampleBuffer:(CMSampleBufferRef)sampleBuffer ofType:(SCStreamOutputType)type
{
	(void)stream;

	if( type != SCStreamOutputTypeScreen || CMSampleBufferIsValid( sampleBuffer ) == false )
	{
		return;
	}

	CFArrayRef attachmentsArray = CMSampleBufferGetSampleAttachmentsArray( sampleBuffer, false );
	if( attachmentsArray == nullptr || CFArrayGetCount( attachmentsArray ) < 1 )
	{
		return;
	}

	NSDictionary* attachments = (__bridge NSDictionary*)CFArrayGetValueAtIndex( attachmentsArray, 0 );
	NSNumber* status = attachments[SCStreamFrameInfoStatus];
	// idle frames (nothing changed) and blank/suspended frames carry no new picture
	if( status == nil || SCFrameStatus( status.integerValue ) != SCFrameStatusComplete )
	{
		return;
	}

	CVImageBufferRef pixelBuffer = CMSampleBufferGetImageBuffer( sampleBuffer );
	if( pixelBuffer == nullptr )
	{
		return;
	}

	m_capture->storeFrame( pixelBuffer, attachments[SCStreamFrameInfoDirtyRects] );
}

- (void)stream:(SCStream*)stream didStopWithError:(NSError*)error
{
	(void)stream;
	m_capture->setError( error, "screen capture stopped" );
	m_capture->running = false;
}

@end



MacOsScreenCapture::MacOsScreenCapture() :
	d( std::make_unique<Private>() )
{
}



MacOsScreenCapture::~MacOsScreenCapture()
{
	stop();
}



bool MacOsScreenCapture::hasPermission()
{
	return CGPreflightScreenCaptureAccess();
}



void MacOsScreenCapture::requestPermission()
{
	CGRequestScreenCaptureAccess();
}



static void captureSizeOfDisplay( CGDirectDisplayID display, int* width, int* height )
{
	// the display's real resolution in pixels (twice the points on Retina displays), but no larger than
	// MaximumCaptureSize on the longer side - enough for a full-screen picture on common classroom displays
	// (1920 x 1200 for a 2880 x 1800 MacBook display) without the data of a full Retina resolution
	const auto bounds = CGDisplayBounds( display );
	double pixelWidth = bounds.size.width;
	double pixelHeight = bounds.size.height;
	if( auto mode = CGDisplayCopyDisplayMode( display ) )
	{
		pixelWidth = double( CGDisplayModeGetPixelWidth( mode ) );
		pixelHeight = double( CGDisplayModeGetPixelHeight( mode ) );
		CGDisplayModeRelease( mode );
	}

	const auto scale = std::min( 1.0, MaximumCaptureSize / std::max( pixelWidth, pixelHeight ) );
	*width = int( std::lround( pixelWidth * scale / 2 ) * 2 );
	*height = int( std::lround( pixelHeight * scale / 2 ) * 2 );
}



void MacOsScreenCapture::captureSize( int* width, int* height )
{
	captureSizeOfDisplay( CGMainDisplayID(), width, height );
}



bool MacOsScreenCapture::start( int framesPerSecond )
{
	stop();

	@autoreleasepool
	{
		dispatch_semaphore_t done = dispatch_semaphore_create( 0 );

		__block SCShareableContent* content = nil;
		__block NSError* contentError = nil;
		[SCShareableContent getShareableContentExcludingDesktopWindows:NO onScreenWindowsOnly:YES
													 completionHandler:^( SCShareableContent* result, NSError* error ) {
			content = result;
			contentError = error;
			dispatch_semaphore_signal( done );
		}];

		if( dispatch_semaphore_wait( done, dispatch_time( DISPATCH_TIME_NOW, StartTimeoutNanoseconds ) ) != 0 )
		{
			d->setError( nil, "timeout while querying the displays" );
			return false;
		}

		if( content == nil )
		{
			// also happens if the Screen Recording permission is missing
			d->setError( contentError, "could not query the displays" );
			return false;
		}

		SCDisplay* mainDisplay = nil;
		for( SCDisplay* display in content.displays )
		{
			if( display.displayID == CGMainDisplayID() )
			{
				mainDisplay = display;
				break;
			}
		}
		if( mainDisplay == nil )
		{
			mainDisplay = content.displays.firstObject;
		}
		if( mainDisplay == nil )
		{
			d->setError( nil, "no display found" );
			return false;
		}

		SCContentFilter* filter = [[SCContentFilter alloc] initWithDisplay:mainDisplay excludingWindows:@[]];

		int captureWidth = 0;
		int captureHeight = 0;
		captureSizeOfDisplay( mainDisplay.displayID, &captureWidth, &captureHeight );

		SCStreamConfiguration* configuration = [[SCStreamConfiguration alloc] init];
		configuration.width = size_t( captureWidth );
		configuration.height = size_t( captureHeight );
		configuration.pixelFormat = kCVPixelFormatType_32BGRA;
		configuration.colorSpaceName = kCGColorSpaceSRGB;
		configuration.minimumFrameInterval = CMTimeMake( 1, std::max( 1, framesPerSecond ) );
		configuration.showsCursor = YES;
		configuration.queueDepth = 4;

		d->queue = dispatch_queue_create( "io.veyon.screencapture", DISPATCH_QUEUE_SERIAL );
		d->output = [[VeyonScreenCaptureOutput alloc] initWithCapture:d.get()];
		d->stream = [[SCStream alloc] initWithFilter:filter configuration:configuration delegate:d->output];

		NSError* outputError = nil;
		if( [d->stream addStreamOutput:d->output type:SCStreamOutputTypeScreen sampleHandlerQueue:d->queue
								 error:&outputError] == NO )
		{
			d->setError( outputError, "could not add the stream output" );
			d->stream = nil;
			d->output = nil;
			return false;
		}

		__block NSError* startError = nil;
		[d->stream startCaptureWithCompletionHandler:^( NSError* error ) {
			startError = error;
			dispatch_semaphore_signal( done );
		}];

		if( dispatch_semaphore_wait( done, dispatch_time( DISPATCH_TIME_NOW, StartTimeoutNanoseconds ) ) != 0 ||
			startError != nil )
		{
			d->setError( startError, "could not start the screen capture" );
			d->stream = nil;
			d->output = nil;
			return false;
		}

		d->running = true;
	}

	return true;
}



void MacOsScreenCapture::stop()
{
	@autoreleasepool
	{
		if( d->stream != nil )
		{
			dispatch_semaphore_t done = dispatch_semaphore_create( 0 );
			[d->stream stopCaptureWithCompletionHandler:^( NSError* ) {
				dispatch_semaphore_signal( done );
			}];
			dispatch_semaphore_wait( done, dispatch_time( DISPATCH_TIME_NOW, StopTimeoutNanoseconds ) );
		}

		d->stream = nil;
		d->output = nil;
		d->running = false;
	}
}



bool MacOsScreenCapture::isRunning() const
{
	return d->running;
}



std::string MacOsScreenCapture::lastError() const
{
	std::lock_guard<std::mutex> lock( d->mutex );
	return d->lastError;
}



bool MacOsScreenCapture::consumeFrame( const FrameConsumer& consumer )
{
	std::lock_guard<std::mutex> lock( d->mutex );

	if( d->hasNewFrame == false )
	{
		return false;
	}

	consumer( d->pixels.data(), d->width, d->height, d->width * 4, d->dirtyRects );

	d->dirtyRects.clear();
	d->hasNewFrame = false;

	return true;
}
