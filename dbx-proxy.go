// SPDX-License-Identifier: GPL-3.0-only
package main

import (
	"bufio"
	"log"
	"net"
	"os"
	"os/exec"
)

const C string = "dbx"

func main() {
	if len(os.Args) != 2 {
		log.Fatal("ERROR: invalid command arguments")
	}

	port := os.Args[1]
	addr := "localhost:" + port
	conn, err := net.Listen("tcp", addr)
	if err != nil {
		log.Fatal(err)
	}
	defer conn.Close()

	for {
		client, err := conn.Accept()
		if err != nil {
			log.Fatal(err)
		}
		go handleClient(client, port)
	}
}

func handleClient(client net.Conn, port string) error {
	log.Print("connected ", client.RemoteAddr())
	defer client.Close()

	netcat := exec.Command("podman", "exec", "--interactive", "--user=1000", C, "netcat", "-N", "localhost", port)
	netcat.Stdin = client
	netcat.Stdout = client

	stderr, err := netcat.StderrPipe()
	if err != nil {
		log.Print(err)
		return err
	}
	defer stderr.Close()

	if err := netcat.Start(); err != nil {
		log.Print(err)
		return err
	}

	scanner := bufio.NewScanner(stderr)
	for scanner.Scan() {
		log.Print("netcat 2>", scanner.Text())
	}

	client.Close()
	if err := netcat.Wait(); err != nil {
		log.Print(err)
		return err
	}

	log.Print("disconnected ", client.RemoteAddr())
	return nil
}
