-- phpMyAdmin SQL Dump
-- version 2.6.4-pl3
-- http://www.phpmyadmin.net
-- 
-- Host: fdb1.awardspace.com
-- Generation Time: Mar 02, 2010 at 12:24 AM
-- Server version: 4.1.18
-- PHP Version: 5.3.1
-- 
-- Database: `hackykid_uftt`
-- 

-- --------------------------------------------------------

-- 
-- Table structure for table `peers`
-- 

CREATE TABLE `peers` (
  `id` int(11) NOT NULL auto_increment,
  `address` varchar(64) NOT NULL default '',
  `port` int(11) NOT NULL default '0',
  `time` datetime NOT NULL default '0000-00-00 00:00:00',
  `type` varchar(64) NOT NULL default '',
  `class` varchar(64) NOT NULL default '',
  PRIMARY KEY  (`id`)
) ENGINE=MyISAM DEFAULT CHARSET=latin1 AUTO_INCREMENT=24460 ;
