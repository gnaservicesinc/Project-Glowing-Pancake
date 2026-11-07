// Development-only native encoding for the original procedural background.
import AppKit
import AVFoundation
import CoreVideo

let args = CommandLine.arguments
guard args.count == 3, let image = NSImage(contentsOfFile: args[1]),
      let source = image.cgImage(forProposedRect: nil, context: nil, hints: nil) else {
    fatalError("usage: create_menu_video.swift source.jpg output.mp4")
}
let output = URL(fileURLWithPath: args[2])
if FileManager.default.fileExists(atPath: output.path) { try FileManager.default.removeItem(at: output) }
let writer = try AVAssetWriter(outputURL: output, fileType: .mp4)
let input = AVAssetWriterInput(mediaType: .video, outputSettings: [
    AVVideoCodecKey: AVVideoCodecType.h264, AVVideoWidthKey: 1280, AVVideoHeightKey: 720,
    AVVideoCompressionPropertiesKey: [AVVideoAverageBitRateKey: 900_000, AVVideoMaxKeyFrameIntervalKey: 48]
])
let adapter = AVAssetWriterInputPixelBufferAdaptor(assetWriterInput: input, sourcePixelBufferAttributes: [
    kCVPixelBufferPixelFormatTypeKey as String: kCVPixelFormatType_32ARGB,
    kCVPixelBufferWidthKey as String: 1280, kCVPixelBufferHeightKey as String: 720,
    kCVPixelBufferCGImageCompatibilityKey as String: true,
    kCVPixelBufferCGBitmapContextCompatibilityKey as String: true
])
writer.add(input)
guard writer.startWriting() else { fatalError("Cannot start video writer: \(String(describing: writer.error))") }
writer.startSession(atSourceTime: .zero)
for frame in 0..<240 {
    while !input.isReadyForMoreMediaData && writer.status == .writing { Thread.sleep(forTimeInterval: 0.002) }
    guard writer.status == .writing, let pool = adapter.pixelBufferPool else { fatalError("Video writer failed") }
    var candidate: CVPixelBuffer?
    guard CVPixelBufferPoolCreatePixelBuffer(nil, pool, &candidate) == kCVReturnSuccess, let buffer = candidate else {
        fatalError("Cannot allocate video frame")
    }
    CVPixelBufferLockBaseAddress(buffer, [])
    let context = CGContext(data: CVPixelBufferGetBaseAddress(buffer), width: 1280, height: 720,
        bitsPerComponent: 8, bytesPerRow: CVPixelBufferGetBytesPerRow(buffer), space: CGColorSpaceCreateDeviceRGB(),
        bitmapInfo: CGImageAlphaInfo.noneSkipFirst.rawValue)!
    let zoom = 1.025 + 0.007 * sin(2 * Double.pi * Double(frame) / 240)
    let width = 1280 * zoom, height = 720 * zoom
    context.draw(source, in: CGRect(x: (1280-width)/2, y: (720-height)/2, width: width, height: height))
    CVPixelBufferUnlockBaseAddress(buffer, [])
    guard adapter.append(buffer, withPresentationTime: CMTime(value: Int64(frame), timescale: 24)) else {
        fatalError("Cannot encode frame: \(String(describing: writer.error))")
    }
}
input.markAsFinished()
let finished = DispatchSemaphore(value: 0)
writer.finishWriting { finished.signal() }
finished.wait()
guard writer.status == .completed else { fatalError("Cannot finish video: \(String(describing: writer.error))") }
