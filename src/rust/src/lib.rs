use libc::{c_int, size_t};
use std::io::{Read, Write};
use std::net::TcpStream;
use std::os::windows::io::FromRawSocket;
use std::sync::Arc;
use threadpool::ThreadPool;

// Global thread pool (initialized once)
static mut THREAD_POOL: Option<ThreadPool> = None;

// Initialize the thread pool with specified number of threads
#[no_mangle]
pub extern "C" fn init_thread_pool(num_threads: size_t) -> c_int {
    unsafe {
        if THREAD_POOL.is_some() {
            return 0; // Already initialized
        }
        
        THREAD_POOL = Some(ThreadPool::new(num_threads as usize));
        return 1; // Success
    }
}

// Process a client socket in the thread pool
#[no_mangle]
pub extern "C" fn process_client_threaded(socket_fd: c_int) -> c_int {
    unsafe {
        if let Some(pool) = &THREAD_POOL {
            // We need to get ownership of the socket to move it to the thread
            // Using unsafe code to convert the raw socket descriptor
            let socket_fd = socket_fd as u32;
            
            pool.execute(move || {
                // Convert the raw socket to a TcpStream
                let socket = match unsafe { TcpStream::from_raw_socket(socket_fd) } {
                    stream => stream,
                };
                
                // Process the client (echo server logic)
                match process_client_connection(socket) {
                    Ok(_) => println!("Client processed successfully"),
                    Err(e) => eprintln!("Error processing client: {}", e),
                }
                
                // The socket will be closed when it goes out of scope
                // No need to explicitly close it
            });
            
            return 1; // Successfully queued
        } else {
            eprintln!("Thread pool not initialized");
            return 0; // Failed
        }
    }
}

// Function to actually process a client connection (echo server logic)
fn process_client_connection(mut socket: TcpStream) -> std::io::Result<()> {
    const BUFFER_SIZE: usize = 1024;
    let mut buffer = [0u8; BUFFER_SIZE];
    
    // Set read timeout to avoid hanging forever
    socket.set_read_timeout(Some(std::time::Duration::from_secs(60)))?;
    
    loop {
        // Read from client
        let bytes_read = match socket.read(&mut buffer) {
            Ok(0) => break, // Connection closed
            Ok(n) => n,
            Err(e) => {
                eprintln!("Error reading from socket: {}", e);
                break;
            }
        };
        
        // Echo the data back
        if let Err(e) = socket.write_all(&buffer[0..bytes_read]) {
            eprintln!("Error writing to socket: {}", e);
            break;
        }
        
        // Log what we received
        println!("Echoed {} bytes", bytes_read);
    }
    
    println!("Client disconnected");
    Ok(())
}

// Shutdown the thread pool properly
#[no_mangle]
pub extern "C" fn shutdown_thread_pool() {
    unsafe {
        // Replace with None to drop the thread pool, which waits for all jobs to complete
        THREAD_POOL = None;
    }
}
