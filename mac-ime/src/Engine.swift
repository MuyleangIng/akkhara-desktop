// The "brain" on a background thread: akkhara-mini through the C++ engine (core/akkhara_engine.cpp,
// the same one as the Windows keyboard, so both give the same top-5).
// Each key press gets a new generation number: older work stops early and its answers are dropped.
import Foundation

final class Engine {
    static let shared = Engine()

    private let queue = DispatchQueue(label: "akkhara.engine", qos: .userInitiated)
    private var handle: OpaquePointer?
    private(set) var ready = false
    private(set) var failed: String?
    private var generation: Int64 = 0  // only changed on the main thread

    /// Load the model in the background (a second or so); typing works meanwhile (Latin is kept).
    func start(modelDir: String? = nil) {
        queue.async {
            guard let dir = modelDir ?? Bundle.main.resourceURL?.appendingPathComponent("model").path else { return }
            var err = [CChar](repeating: 0, count: 512)
            let h = akk_open(dir, &err, err.count)
            DispatchQueue.main.async {
                if let h = h {
                    self.handle = h
                    self.ready = true
                } else {
                    self.failed = String(cString: err)
                    NSLog("Akkhara: model failed to load: \(self.failed ?? "")")
                }
            }
        }
    }

    /// Ask for suggestions for `latin`. `update` is called on the main thread: first the fast #1
    /// (final = false), then the top-5 (final = true). Nothing is called for outdated requests.
    @discardableResult
    func request(_ latin: String, update: @escaping (_ items: [String], _ final: Bool) -> Void) -> Int64 {
        generation += 1
        let gen = generation
        guard let h = handle else { return gen }
        akk_set_generation(h, gen)
        queue.async {
            var buf = [CChar](repeating: 0, count: 8192)
            if akk_quick(h, gen, latin, &buf, buf.count) != 0 {
                let one = String(cString: buf)
                DispatchQueue.main.async { if gen == self.generation { update([one], false) } }
            }
            if akk_candidates(h, gen, latin, &buf, buf.count) > 0 {
                let items = String(cString: buf).components(separatedBy: "\n")
                DispatchQueue.main.async { if gen == self.generation { update(items, true) } }
            }
        }
        return gen
    }

    /// Stop any work in progress (the text was inserted or dropped).
    func cancel() {
        generation += 1
        if let h = handle { akk_set_generation(h, generation) }
    }

    /// Stop and wait for the background work (before the program quits).
    func shutdown() {
        cancel()
        queue.sync {}
    }

    /// #1 right now (used when Enter comes before the suggestions are back).
    func quickNow(_ latin: String) -> String? {
        guard let h = handle else { return nil }
        generation += 1
        akk_set_generation(h, generation)
        var buf = [CChar](repeating: 0, count: 8192)
        return akk_quick(h, generation, latin, &buf, buf.count) != 0 ? String(cString: buf) : nil
    }
}
