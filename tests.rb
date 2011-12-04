#!/usr/bin/env ruby

require "rubygems"
require "json"

BASE = File.expand_path(File.join(File.dirname(__FILE__), ".."))
FAS = File.join(BASE, "fas")

TEST_CASES = ARGV.length > 0 ? ARGV : Dir["#{BASE}/testcases/*.data"]

def fas(file)
  results = %x{#{FAS} < #{file}}
  score, ordering = results.split("\n");

  { :score => score.gsub(/Score: */, "").to_f,
    :ordering => ordering.gsub(/^.+: */, "").gsub(/\[|\]/, "").split.map{|x| x.to_i}
  }
end

TEST_CASES.sort!

TEST_CASES.each do |test|
  score_file = test.gsub(/.data$/, ".json")

  best_score = 0
  best_run = []
  write_back = false

  if File.exists?(score_file)
    data = JSON.parse(IO.read(score_file))
    best_score = data["score"]
    best_run = data["ordering"]
  end

  scores = []

  run_count = 1

  start = Time.now
  run_count.times do 
    ft = fas(test)
    scores << ft[:score]

    if ft[:score] > best_score
      best_score = ft[:score]
      best_run = ft[:ordering]
      write_back = true
    end
  end
  
  duration = (Time.now - start) / run_count

  scores.sort!

  best_fraction_of_best = scores[-1] / best_score

  puts [File.basename(test), IO.read(test).split("\n").count, duration, scores[-1], best_fraction_of_best].join("\t")
  if write_back
    File.open(score_file, "w"){|o| o.puts(JSON.pretty_generate({:score => best_score, :ordering => best_run})) }
  end
end