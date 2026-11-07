fn main() {
    let mut entropy = [0u8; 32];
    getrandom::fill(&mut entropy).expect("QNX entropy read failed");
    println!("FORGE_QNX_GETRANDOM_OK bytes={}", entropy.len());
    let result = std::thread::spawn(|| 20 + 22).join().expect("worker panicked");
    assert_eq!(result, 42);
    println!("FORGE_QNX_RUST_STD_OK thread=42 pid={}", std::process::id());
}
